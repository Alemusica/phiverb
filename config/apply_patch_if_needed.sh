#!/usr/bin/env bash
set -euo pipefail

PATCH_FILE="$1"
SENTINEL="$2"
GIT_BIN="${3:-git}"

if [[ -f "$SENTINEL" ]]; then
    exit 0
fi

if "$GIT_BIN" apply --check "$PATCH_FILE" >/dev/null 2>&1; then
    "$GIT_BIN" apply "$PATCH_FILE"
    touch "$SENTINEL"
    exit 0
fi

if "$GIT_BIN" apply -R --check "$PATCH_FILE" >/dev/null 2>&1; then
    touch "$SENTINEL"
    exit 0
fi

echo "[wayverb] Failed to apply patch $PATCH_FILE" >&2
exit 1
