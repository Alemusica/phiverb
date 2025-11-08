#!/usr/bin/env bash
set -euo pipefail

TITLE=${1:-"WIP: checkpoint"}

git add -A
if git diff --cached --quiet; then
    echo "[checkpoint] nessuna modifica da committare."
else
    git commit -m "$TITLE" || true
fi

if git config remote.origin.url >/dev/null 2>&1; then
    git push -u origin HEAD || true
else
    echo "[checkpoint] remote 'origin' assente, push saltato."
fi

if command -v gh >/dev/null 2>&1; then
    if gh pr view >/dev/null 2>&1; then
        gh pr comment --body "Checkpoint automatico (token-guard)." || true
    else
        gh pr create --fill --draft --label wip || true
    fi
else
    echo "[checkpoint] GitHub CLI non disponibile; aggiorna la PR manualmente se serve."
fi

echo "[checkpoint] completato"
