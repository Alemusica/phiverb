# Control-Room Dev Diary

Each agent must record their intent and major steps while working. Use

```
scripts/agents/log_note.sh "short description"
```

The script appends a timestamped entry (with branch info) to
`logs/control_room/<agent>.md`. Log at minimum:

1. Inizio sessione (cosa farai, branch, Action Plan ID).
2. Ogni milestone rilevante (nuovi test, regressioni eseguite, bug trovati).
3. Prima di consegnare log/PR.

Reference the relevant entries in your PR (see template) and in `docs/action_plan.md`
updates so the Control Room can audit progress in real time.
