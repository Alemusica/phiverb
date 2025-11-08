#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-metal}"

mkdir -p "$BUILD_DIR"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DWAYVERB_ENABLE_METAL=ON
cmake --build "$BUILD_DIR" -j

echo "Metal build complete. To run with Metal backend (scaffold):"
echo "  WAYVERB_METAL=1 $BUILD_DIR/bin/apple_silicon_regression"
echo "or launch the app binary with WAYVERB_METAL=1 (once Xcode target links the metal module)."

