#!/usr/bin/env bash
set -euo pipefail

RESERVE=${RESERVE:-12000}
YELLOW=${YELLOW:-0.60}
RED=${RED:-0.80}
export TOKEN_GUARD_YELLOW="$YELLOW"
export TOKEN_GUARD_RED="$RED"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHECKPOINT_SCRIPT="${SCRIPT_DIR}/checkpoint_or_pr.sh"

tmp_file="$(mktemp)"
cleanup() {
    rm -f "$tmp_file"
}
trap cleanup EXIT

if [ "$#" -gt 0 ]; then
    cat "$@" > "$tmp_file"
else
    cat - > "$tmp_file"
fi

chars=$(wc -c < "$tmp_file" | tr -d '[:space:]')
chars=${chars:-0}
tok_est=$(( (chars + 3) / 4 ))

WIN=${AGENT_TOKEN_WINDOW:-128000}
USED=${AGENT_TOKENS_USED:-0}

export TOKEN_GUARD_WIN="$WIN"
export TOKEN_GUARD_USED="$USED"
export TOKEN_GUARD_EST="$tok_est"
export TOKEN_GUARD_RESERVE="$RESERVE"

PERC=$(python3 - <<'PY'
import os
win=float(os.environ.get("TOKEN_GUARD_WIN","0") or 0)
used=float(os.environ.get("TOKEN_GUARD_USED","0") or 0)
print("0.0" if win <= 0 else f"{used / win:.4f}")
PY
)
export TOKEN_GUARD_PERC="$PERC"

PCT_DISPLAY=$(python3 - <<'PY'
import os, math
perc=float(os.environ.get("TOKEN_GUARD_PERC","0") or 0)
print(f"{perc*100:.0f}")
PY
)

NEEDED=$(python3 - <<'PY'
import os, math
used=float(os.environ.get("TOKEN_GUARD_USED","0") or 0)
est=float(os.environ.get("TOKEN_GUARD_EST","0") or 0)
reserve=float(os.environ.get("TOKEN_GUARD_RESERVE","0") or 0)
print(int(math.ceil(used + est + reserve)))
PY
)
export TOKEN_GUARD_NEEDED="$NEEDED"

echo "[token-guard] used=${USED}/${WIN} (~${PCT_DISPLAY}%); est_next=${tok_est}; reserve=${RESERVE}; projected=${NEEDED}"

STATUS=$(python3 - <<'PY'
import os
perc=float(os.environ.get("TOKEN_GUARD_PERC","0") or 0)
yellow=float(os.environ.get("TOKEN_GUARD_YELLOW","0.60"))
red=float(os.environ.get("TOKEN_GUARD_RED","0.80"))
print(0 if perc < yellow else (1 if perc < red else 2))
PY
)

case "$STATUS" in
  0) echo "[token-guard] status=green" ;;
  1) echo "[token-guard] status=yellow — plan checkpoint soon." ;;
  2) echo "[token-guard] status=red — checkpoint strongly recommended." ;;
esac

RISK=$(python3 - <<'PY'
import os
win=float(os.environ.get("TOKEN_GUARD_WIN","0") or 0)
needed=float(os.environ.get("TOKEN_GUARD_NEEDED","0") or 0)
if win <= 0:
    print("0")
else:
    print("1" if needed >= win else "0")
PY
)

if [ "$RISK" = "1" ]; then
    echo "[token-guard] LOW BUDGET → checkpoint & draft PR"
    if [ -x "$CHECKPOINT_SCRIPT" ]; then
        "$CHECKPOINT_SCRIPT" "WIP: checkpoint (low token budget)"
    else
        echo "[token-guard] checkpoint script not found at $CHECKPOINT_SCRIPT" >&2
        exit 3
    fi
    exit 3
fi
