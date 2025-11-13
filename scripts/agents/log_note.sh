#!/usr/bin/env bash
set -euo pipefail

# Append a timestamped note to the control-room dev diary.
# Usage:
#   scripts/agents/log_note.sh "message"
#   echo "multi-line" | scripts/agents/log_note.sh

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOG_DIR="${LOG_DIR:-$ROOT_DIR/logs/control_room}"
AGENT_ID="${AGENT_ID:-${USER:-unknown}}"
BRANCH="$(git -C "$ROOT_DIR" rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"

mkdir -p "$LOG_DIR"
LOG_FILE="${LOG_DIR}/${AGENT_ID}.md"

if [[ $# -gt 0 ]]; then
  NOTE="$*"
else
  NOTE="$(cat)"
fi

if [[ -z "${NOTE// }" ]]; then
  echo "[log_note] refusing to record empty note" >&2
  exit 1
fi

timestamp="$(date +"%Y-%m-%d %H:%M:%S %Z")"

{
  printf "### %s — branch %s\n\n" "$timestamp" "$BRANCH"
  printf "%s\n\n" "$NOTE"
} >> "$LOG_FILE"

echo "[log_note] wrote entry to $LOG_FILE"
