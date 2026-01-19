#!/usr/bin/env bash
set -euo pipefail
PROJECT_DIR="$(pwd)"
IMAGE_NAME="sparsebitset:test"

# Build docker image
docker build -t "$IMAGE_NAME" .

# Run container and execute the test runner
docker run --rm -v "$PROJECT_DIR":/workspace -w /workspace "$IMAGE_NAME" /workspace/run_flow_tests.sh "$@"
