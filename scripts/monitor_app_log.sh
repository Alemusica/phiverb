#!/usr/bin/env bash
set -euo pipefail

# Monitors the latest Wayverb app log and extracts key diagnostics
# into a rolling monitor log with timestamps.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_LOG_DIR="$ROOT_DIR/build/logs/app"
MON_DIR="$APP_LOG_DIR"

mkdir -p "$APP_LOG_DIR"

latest_log() {
  ls -t "$APP_LOG_DIR"/wayverb-*.log 2>/dev/null | head -n 1
}

DEFAULT_LOG="${WAYVERB_LAST_APP_LOG:-$(latest_log)}"
LOG_FILE="${1:-$DEFAULT_LOG}"
if [[ -z "${LOG_FILE:-}" || ! -f "$LOG_FILE" ]]; then
  echo "No app log found in $APP_LOG_DIR (WAYVERB_LAST_APP_LOG='${WAYVERB_LAST_APP_LOG:-}') ." >&2
  echo "Launch via tools/run_wayverb.sh (which now always tees into build/logs/app) or pass a log file path explicitly." >&2
  exit 1
fi

TS="$(date +"%Y%m%d-%H%M%S")"
OUT="$MON_DIR/monitor-$TS.log"
PID_FILE="$MON_DIR/monitor-$TS.pid"

PATTERN='(render error|All channels are silent|silent IR|non-zero samples|max\|x\||nan|INF|\[waveguide\]|\[combined\]|clCreateBuffer|error|failed)'
STREAM_STDOUT="${WAYVERB_MONITOR_STDOUT:-1}"

(
  echo "[monitor] watching $LOG_FILE" >> "$OUT"
  if [[ "$STREAM_STDOUT" == "1" ]]; then
    tail -F "$LOG_FILE" 2>/dev/null | awk -v pat="$PATTERN" 'BEGIN{IGNORECASE=1} { if ($0 ~ pat) { cmd="date +%Y-%m-%dT%H:%M:%S"; cmd | getline ts; close(cmd); printf("%s %s\n", ts, $0); fflush(); } }' | tee -a "$OUT"
  else
    tail -F "$LOG_FILE" 2>/dev/null | awk -v pat="$PATTERN" 'BEGIN{IGNORECASE=1} { if ($0 ~ pat) { cmd="date +%Y-%m-%dT%H:%M:%S"; cmd | getline ts; close(cmd); printf("%s %s\n", ts, $0); fflush(); } }' >> "$OUT"
  fi
) & echo $! > "$PID_FILE"

echo "monitor_log=$OUT"
echo "monitor_pid=$(cat "$PID_FILE")"
echo "To stop: kill \$(cat "$PID_FILE")"
