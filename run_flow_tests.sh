#!/usr/bin/env bash

set -euo pipefail

# Directory of this script / project root
if [ -n "${BASH_SOURCE[0]:-}" ]; then
  PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
else
  PROJECT_DIR="$(pwd)"
fi
VENV_DIR="${VENV_DIR:-$HOME/.cache/sparsebitset_rltest_venv}"
PYTHON_BIN="$(command -v python3 || command -v python || true)"

# Create virtual environment if it does not exist
if [[ ! -d "$VENV_DIR" ]]; then
  if [ -n "$PYTHON_BIN" ]; then
    echo "[+] Creating virtual environment in $VENV_DIR"
    "$PYTHON_BIN" -m venv "$VENV_DIR"
  else
    echo "[!] No python interpreter found (python3/python). Continuing and hoping system python is available."
  fi
fi

# Prefer using the venv's python for installing and running RLTest
VENV_PY="$VENV_DIR/bin/python"
ACTIVATED=0
if [ -x "$VENV_PY" ]; then
  echo "[+] Using virtualenv python: $VENV_PY"
  "$VENV_PY" -m pip install --quiet --upgrade pip
  "$VENV_PY" -m pip install --quiet RLTest redis
  ACTIVATED=1
fi

if [ "$ACTIVATED" -eq 0 ]; then
  echo "[!] Virtualenv not available; falling back to system python for pip installs (user site)"
  if [ -n "$PYTHON_BIN" ]; then
    "$PYTHON_BIN" -m pip install --user --quiet --upgrade pip || true
    "$PYTHON_BIN" -m pip install --user --quiet RLTest redis || true
  else
    echo "[!] No python interpreter available to install RLTest. Aborting."
    exit 1
  fi
fi

# Ensure bitset.so exists (build if necessary)
if [[ ! -f "$PROJECT_DIR/bitset.so" ]]; then
  echo "[+] Building bitset.so via make"
  make -C "$PROJECT_DIR"
fi

# Run test matrix and save RLTest logs for auditing
RUNS=("" "--use-aof" "--use-slaves" "--use-slaves --use-aof")
LOG_DIR="$PROJECT_DIR/tests/flow/rltest_logs"
mkdir -p "$LOG_DIR"

for opts in "${RUNS[@]}"; do
  echo "[+] Running RLTest with options: $opts $*"
  cd "$PROJECT_DIR/tests/flow/"

  # Snapshot existing files to detect new/changed files after RLTest run
  SNAPSHOT_DIR=$(mktemp -d)
  find . -maxdepth 1 -mindepth 1 -printf "%P\n" > "$SNAPSHOT_DIR/before.txt"

  # Run RLTest (this will create logs under this directory)
  if [ "$ACTIVATED" -eq 1 ]; then
    "$VENV_PY" -m RLTest --clear-logs --randomize-ports --module "$PROJECT_DIR/bitset.so" --enable-module-command --enable-debug-command --no-progress $opts "$@"
  else
    "$PYTHON_BIN" -m RLTest --clear-logs --randomize-ports --module "$PROJECT_DIR/bitset.so" --enable-module-command --enable-debug-command --no-progress $opts "$@"
  fi

  # Determine newly created/modified files
  find . -maxdepth 1 -mindepth 1 -printf "%P\n" > "$SNAPSHOT_DIR/after.txt"
  NEW_FILES=$(comm -13 <(sort "$SNAPSHOT_DIR/before.txt") <(sort "$SNAPSHOT_DIR/after.txt") || true)

  # Prepare a timestamped, sanitized folder name for this run
  TS=$(date -u +%Y%m%dT%H%M%SZ)
  SAFE_OPTS=$(echo "$opts" | tr ' /' '__' | sed 's/^-*//; s/[^A-Za-z0-9_]/_/g')
  DEST="$LOG_DIR/${TS}${SAFE_OPTS:+_}${SAFE_OPTS}"
  mkdir -p "$DEST"

  # If RLTest writes its logs to subfolders (e.g., rlt-*), copy them as well
  # Also copy any new top-level files detected
  if [ -n "$NEW_FILES" ]; then
    echo "$NEW_FILES" | while read -r f; do
      [ -z "$f" ] && continue
      if [ -d "$f" ]; then
        cp -a "$f" "$DEST/"
      else
        cp -a "$f" "$DEST/"
      fi
    done
  fi

  # Always try to copy common RLTest directories if present
  for candidate in rlt_* logs; do
    if [ -e "$candidate" ]; then
      cp -a "$candidate" "$DEST/" || true
    fi
  done

  # Save the test command and options for traceability
  echo "RLTest options: $opts $*" > "$DEST/command.txt"

  # Clean up snapshot
  rm -rf "$SNAPSHOT_DIR"

  cd - >/dev/null
done

# Deactivate venv if we activated it
if [ "$ACTIVATED" -eq 1 ]; then
  deactivate || true
fi

echo "[+] Tests completed"
