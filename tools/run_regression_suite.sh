#!/usr/bin/env bash
set -euo pipefail

# Runs bin/apple_silicon_regression over a set of scenes with fallback disabled.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REG_BIN="$ROOT_DIR/bin/apple_silicon_regression"
SCENE_DIR="$ROOT_DIR/geometrie_wayverb"
LOG_DIR="$ROOT_DIR/build/logs/regressions"
mkdir -p "$LOG_DIR"

if [[ ! -x "$REG_BIN" ]]; then
  echo "Regression binary not found at $REG_BIN. Build it first." >&2
  exit 1
fi

SCENES=()
EXTRA_ARGS=()
if [[ $# -gt 0 ]]; then
  SCENES=("$@")
else
  mapfile -t SCENES < <(ls "$SCENE_DIR"/*.obj 2>/dev/null)
fi

if [[ ${#SCENES[@]} -eq 0 ]]; then
  echo "No scenes provided and none found under $SCENE_DIR" >&2
  exit 1
fi

TS="$(date +"%Y%m%d-%H%M%S")"
OUT_LOG="$LOG_DIR/regression-$TS.log"
echo "[regression] starting suite at $TS" | tee "$OUT_LOG"

export WAYVERB_ALLOW_SILENT_FALLBACK=0

for scene in "${SCENES[@]}"; do
  echo "[regression] Scene: $scene" | tee -a "$OUT_LOG"
  if ! "$REG_BIN" --scene "$scene" "${EXTRA_ARGS[@]}" 2>&1 | tee -a "$OUT_LOG"; then
    echo "[regression] FAILED for scene $scene" | tee -a "$OUT_LOG"
    exit 2
  fi
done

echo "[regression] suite completed successfully" | tee -a "$OUT_LOG"
echo "log=$OUT_LOG"
