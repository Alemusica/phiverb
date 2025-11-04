#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_PATH="$ROOT_DIR/wayverb/Builds/MacOSX/build/Debug/wayverb.app/Contents/MacOS/wayverb"

if [[ ! -x "$APP_PATH" ]]; then
    echo "Wayverb binary not found at: $APP_PATH" >&2
    echo "Build it first with: xcodebuild -scheme \"wayverb (App)\" -configuration Debug -derivedDataPath wayverb/Builds/MacOSX/build/xcode" >&2
    exit 1
fi

export CL_LOG_ERRORS="${CL_LOG_ERRORS:-stdout}"

echo "Launching Wayverb from shell..."
echo "  binary : $APP_PATH"
echo "  CL_LOG_ERRORS=${CL_LOG_ERRORS}"

exec "$APP_PATH" "$@"
