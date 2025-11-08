#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HEADER_PATH="${ROOT_DIR}/wayverb/Source/build_id_override.h"

git_desc="unknown"
git_branch="unknown"
if command -v git >/dev/null 2>&1; then
    if desc=$(git -C "$ROOT_DIR" describe --always --dirty 2>/dev/null); then
        git_desc="$desc"
    fi
    if branch=$(git -C "$ROOT_DIR" rev-parse --abbrev-ref HEAD 2>/dev/null); then
        git_branch="$branch"
    fi
fi

timestamp="$(date +%Y-%m-%dT%H:%M:%S%z)"

escape() {
    printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'
}

git_desc_escaped="$(escape "$git_desc")"
git_branch_escaped="$(escape "$git_branch")"
timestamp_escaped="$(escape "$timestamp")"

cat > "$HEADER_PATH" <<EOF
#pragma once
#define WAYVERB_BUILD_GIT_DESC "${git_desc_escaped}"
#define WAYVERB_BUILD_GIT_BRANCH "${git_branch_escaped}"
#define WAYVERB_BUILD_TIMESTAMP "${timestamp_escaped}"
EOF

echo "[write_build_id_header] wrote \${WAYVERB_BUILD_*} macros to $HEADER_PATH"
