#!/usr/bin/env bash

set -euo pipefail

# Directory of this script / project root
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$PROJECT_DIR/.venv_rltest"
PYTHON_BIN="$(command -v python3)"

# Create virtual environment if it does not exist
if [[ ! -d "$VENV_DIR" ]]; then
  echo "[+] Creating virtual environment in $VENV_DIR"
  "$PYTHON_BIN" -m venv "$VENV_DIR"
fi

# Activate venv
source "$VENV_DIR/bin/activate"

# Upgrade pip silently and install RLTest (and redis-py)
python -m pip install --quiet --upgrade pip
python -m pip install --quiet RLTest redis

# Ensure bitset.so exists (build if necessary)
if [[ ! -f "$PROJECT_DIR/bitset.so" ]]; then
  echo "[+] Building bitset.so via make"
  make -C "$PROJECT_DIR"
fi

# Run test matrix
RUNS=("" "--use-aof" "--use-slaves" "--use-slaves --use-aof")
for opts in "${RUNS[@]}"; do
  echo "[+] Running RLTest with options: $opts $*"
  cd "$PROJECT_DIR/tests/flow/"
  python -m RLTest --clear-logs --module "$PROJECT_DIR/bitset.so" --enable-module-command --enable-debug-command --no-progress $opts "$@"
  cd - >/dev/null
done

# Deactivate venv
deactivate

echo "[+] Tests completed" 