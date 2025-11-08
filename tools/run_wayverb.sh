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

LOG_DIR="${ROOT_DIR}/build/logs/app"
mkdir -p "$LOG_DIR"

timestamp="$(date +%Y%m%d-%H%M%S)"
LOG_FILE_DEFAULT="${LOG_DIR}/wayverb-${timestamp}.log"
LOG_FILE="${WAYVERB_APP_LOG:-$LOG_FILE_DEFAULT}"
LOG_DIRNAME="$(cd "$(dirname "$LOG_FILE")" && pwd)"
LOG_BASENAME="$(basename "$LOG_FILE")"
mkdir -p "$LOG_DIRNAME"
LOG_FILE="${LOG_DIRNAME}/${LOG_BASENAME}"

ln -sf "$LOG_FILE" "${LOG_DIR}/wayverb-latest.log"
export WAYVERB_LAST_APP_LOG="$LOG_FILE"
export WAYVERB_APP_LOG_PATH="$LOG_FILE"

echo "Launching Wayverb from shell..."
echo "  binary : $APP_PATH"
echo "  CL_LOG_ERRORS=${CL_LOG_ERRORS}"
echo "  log    : $LOG_FILE"
echo
echo "stdout/stderr will be tee'd into the log; press Ctrl+C to stop the app (and logging)."

"$APP_PATH" "$@" 2>&1 | tee "$LOG_FILE"
