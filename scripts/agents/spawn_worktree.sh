#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
  echo "Usage: $0 <relative-path> <new-branch> [base-branch]" >&2
  echo "Example: $0 ../PhiVerb-audio audio/spatial-plan infra/control-room" >&2
  exit 1
fi

ROOT_DIR="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [[ -z "$ROOT_DIR" ]]; then
  echo "[spawn_worktree] Run inside a git repository." >&2
  exit 2
fi

TARGET_PATH="$1"
BRANCH_NAME="$2"
BASE_BRANCH="${3:-infra/control-room}"

echo "[spawn_worktree] Adding worktree $TARGET_PATH for branch $BRANCH_NAME (base $BASE_BRANCH)"
git worktree add -b "$BRANCH_NAME" "$TARGET_PATH" "$BASE_BRANCH"

if command -v code >/dev/null 2>&1; then
  echo "[spawn_worktree] Opening VS Code window for $TARGET_PATH"
  code --new-window "$TARGET_PATH" >/dev/null 2>&1 || true
else
  echo "[spawn_worktree] VS Code (code) not found; open $TARGET_PATH manually." >&2
fi

cat <<INFO
[spawn_worktree] Done. Next steps:
  1. cd $TARGET_PATH
  2. git pull origin ${BASE_BRANCH}
  3. Read docs/agent_runbook.md, docs/AGENT_PROMPTS.md, .github/pull_request_template.md, logs/control_room/README.md
  4. Record the first diary entry via scripts/agents/log_note.sh "…"
INFO
