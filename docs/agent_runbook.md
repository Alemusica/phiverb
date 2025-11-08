# Agent Runbook

Operational contract for every agent (raytracer `PhiVerb-rt`, waveguide `PhiVerb-dwm`) working on Wayverb. Keep this file open while you work; update it whenever process requirements change. Legacy docs (es. materiale originale di Reuben) sono solo “mappe” storiche: consultale per capire come siamo arrivati qui, ma non per ripristinare workflow/test superati. Implementazioni e QA devono sempre seguire gli standard 2025 descritti in Runbook + Action Plan.

## 1. Repositories, Branches, Worktrees
- Tutto il lavoro avviene in worktree dedicati: `infra/control-room` resta solo per documentazione/processo.
- Usa lo script `scripts/agents/spawn_worktree.sh <path> <branch> [base]` per creare nuovi ambienti e aprirli in VS Code (default base = `infra/control-room`).
  - Esempio: `scripts/agents/spawn_worktree.sh ../PhiVerb-rt rt/AP-RT-002-diffuse-rain`.
  - Dopo la creazione: `cd` nel worktree, esegui `git pull origin <base>`, leggi i file richiesti e registra subito la prima nota (`log_note.sh`).
- I branch devono iniziare con il prefisso del modulo (`rt/`, `dwm/`, `audio/`, ecc.) e includere l’ID dell’Action Plan.
- Se servono commit storici, usa `git cherry-pick` e documentalo nella PR (vedi §5).

## 2. Mandatory Tooling Loop
Run everything from the workspace root unless specified.

| Action | Command |
| --- | --- |
| Build tools/app | `cmake -S . -B build && cmake --build build -j` |
| Launch UI with logs | `CL_LOG_ERRORS=stdout tools/run_wayverb.sh` |
| Monitor logs | `scripts/monitor_app_log.sh` (uses `WAYVERB_LAST_APP_LOG`) |
| Headless regression | `tools/run_regression_suite.sh [scenes...]` (auto sets `WAYVERB_ALLOW_SILENT_FALLBACK=0`) |

Rules:
1. Every UI or regression run must produce a log under `build/logs/` and you must reference that path in your PR/template.
2. When reproducing “silent IR” or NaN issues, attach the monitoring log plus the app log and note the build label printed by `tools/run_wayverb.sh`.
3. Store sanitized/scaled meshes under `geometrie_wayverb/` and reference them (`--scene geometri_wayverb/<name>.obj`) to keep history reproducible.

## 3. Checkpoints, Token Guard, Runner Tag
1. Before writing >1k tokens, run `scripts/agents/token_guard.sh <file>`; follow its advice (yellow → plan checkpoint, red → mandatory checkpoint).
2. Checkpoint cadence:
   - 20 % progress: open a draft PR.
   - 60 %: run `scripts/agents/checkpoint_or_pr.sh "WIP: checkpoint"` (push branch + comment).
   - 80 %: PR must include Action Plan ID and log evidence.
   - 90 %: final checkpoint then stop unless the maintainer assigns more work.
3. CI runner guard: when the macOS Metal runner is online, comment `runner=macos-metal ready` on the PR so the workflow can start. Missing tag = failed CI.

### Failure/debug policy
- Non superare 5 tentativi consecutivi sullo stesso test/failure senza nuova evidenza: se dopo la quinta iterazione il problema persiste, apri un’ASK (`docs/archeology.md`) o escalalo al Control Room con i log (`build/logs/...`) e le note del diario. Solo dopo la risposta riprendi i tweak.

## 4. Control-Room Dev Diary
- Ogni agente deve annotare le decisioni significative nel diario sotto `logs/control_room/`.
- Usa lo script `scripts/agents/log_note.sh "messaggio"` (oppure pipando testo): registra timestamp, branch e nota.
- A inizio sessione logga il piano di lavoro; logga di nuovo dopo ogni milestone (nuovi test, regressioni, issue).
- Cita sempre le entry rilevanti nel template PR (vedi sezione successiva).
- I file di processo (`docs/agent_runbook.md`, `docs/AGENT_PROMPTS.md`, `docs/archeology.md`, `docs/action_plan.md`, `.github/pull_request_template.md`) sono read-only nei worktree: se risultano modificati, esegui `git checkout -- <file>` prima di fare `git pull origin infra/control-room`; non vanno mai mergiati manualmente.

## 5. PR Template Fields (enforced)
Every PR must fill out:
1. **Branch**: `rt/...` or `dwm/...`.
2. **Action Plan ID**: e.g. `AP-RT-001`.
3. **Docs cited**:
   - Raytracer: `docs_source/boundary.md` section “Geometric Implementation → 2025 status / design targets”.
   - Waveguide: `docs_source/boundary.md` section “DWM Implementation → 2025 status / design targets”.
4. **Logs**: link to `build/logs/app/...` and/or `build/logs/regressions/...`.
5. **Regression commands**: exact CLI(s) executed.
6. **Reused commits**: list hashes cherry-picked from legacy branches (e.g. `infra/control-room`) to guarantee we do not lose prior work.

PRs missing any field are not reviewed.

## 6. Salvaging & Tracking Progress
To keep previous efforts:
1. Identify useful commits (BRDF fixes, guard-tag instrumentation, etc.) and cherry-pick or merge them into your `rt/...` or `dwm/...` branch.
2. In `docs/action_plan.md`, mark the relevant checklist item as completed and note `(imported from <commit>)`.
3. Update `docs/archeology.md` “Cluster principali” with a short entry describing what legacy work was retained and where it now lives.

Quando completi una voce della checklist in `docs/action_plan.md`, apri il file e sostituisci `[ ]` con `[x]`, citando i log (`build/logs/...`) e il commit relativo. Registra la chiusura anche nel dev diary.

## 7. Evidence Requirements
- **Raytracer agent** must attach:
  - Regression log showing `raytracer/tests/reverb_tests` passing.
  - Shoebox ISM↔RT comparison logs once implemented.
- **Waveguide agent** must attach:
  - `tools/run_regression_suite.sh` output (with `WAYVERB_ALLOW_SILENT_FALLBACK=0`).
  - Any mesh/tooling logs when adjusting `geometrie_wayverb/`.

Each log reference should include the relative file path plus timestamp.

## 8. Escalation & External Research
- If you hit a blocker needing external references, open an “ASK” entry (see `docs/archeology.md`) and ping `@creator` with the exact question plus why it is blocking the Action Plan item.
- Do not proceed without that response when the information is blocker-critical.
 - Quando ricevi la risposta, il Control Room archivia il testo completo in `docs/ask/ASK-<date>-<topic>.md`; rileggi il file e cita il path nella nota del diario.

## 9. Document Maintenance
- Whenever you change process, update both this Runbook and `docs/AGENT_PROMPTS.md`.
- Keep `docs/action_plan.md` and `docs/archeology.md` synchronized with reality—these are the canonical progress trackers.

Following this runbook is part of the review checklist; deviations require maintainer approval.
