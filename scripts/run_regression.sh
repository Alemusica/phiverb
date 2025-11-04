#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
SCENE_PATH="${1:-$ROOT_DIR/assets/test_geometry/pyramid_twisted_minor.obj}"
LOG_DIR="$BUILD_DIR/logs"
TIMESTAMP="$(date +"%Y%m%d-%H%M%S")"
LOG_FILE="$LOG_DIR/regression-$TIMESTAMP.log"

mkdir -p "$BUILD_DIR"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" >/dev/null
cmake --build "$BUILD_DIR" --target apple_silicon_regression >/dev/null

mkdir -p "$LOG_DIR"

BINARY_PATH="$BUILD_DIR/bin/apple_silicon_regression"
if [[ -d "$BINARY_PATH" ]]; then
    if [[ -x "$BINARY_PATH/apple_silicon_regression" ]]; then
        BINARY_PATH="$BINARY_PATH/apple_silicon_regression"
    else
        BINARY_PATH="$BINARY_PATH/${BINARY_PATH##*/}"
    fi
fi

echo "Running regression with scene: $SCENE_PATH"
echo "Logs: $LOG_FILE"

"$BINARY_PATH" "$SCENE_PATH" | tee "$LOG_FILE"
