# Agent Runbook

Operational contract for every agent (raytracer `PhiVerb-rt`, waveguide `PhiVerb-dwm`) working on Wayverb. Keep this file open while you work; update it whenever process requirements change.

## 1. Repositories, Branches, Worktrees
- Clone once, then create the mandated worktrees:
  - `git worktree add ../PhiVerb-rt rt/<feature>` (raytracer agent).
  - `git worktree add ../PhiVerb-dwm dwm/<feature>` (waveguide agent).
- All development happens in those worktrees/branches; `infra/control-room` stays untouched except for documentation updates.
- Each branch name must start with `rt/` or `dwm/` and reference an Action Plan ID (e.g. `rt/AP-RT-002-diffuse-rain`).
- If you need code from `infra/control-room`, cherry-pick it into your branch and cite the original commits in your PR checklist (see §4).

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

## 4. PR Template Fields (enforced)
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

## 5. Salvaging Existing Work
To keep previous efforts:
1. Identify useful commits (BRDF fixes, guard-tag instrumentation, etc.) and cherry-pick or merge them into your `rt/...` or `dwm/...` branch.
2. In `docs/action_plan.md`, mark the relevant checklist item as completed and note `(imported from <commit>)`.
3. Update `docs/archeology.md` “Cluster principali” with a short entry describing what legacy work was retained and where it now lives.

## 6. Evidence Requirements
- **Raytracer agent** must attach:
  - Regression log showing `raytracer/tests/reverb_tests` passing.
  - Shoebox ISM↔RT comparison logs once implemented.
- **Waveguide agent** must attach:
  - `tools/run_regression_suite.sh` output (with `WAYVERB_ALLOW_SILENT_FALLBACK=0`).
  - Any mesh/tooling logs when adjusting `geometrie_wayverb/`.

Each log reference should include the relative file path plus timestamp.

## 7. Escalation & External Research
- If you hit a blocker needing external references, open an “ASK” entry (see `docs/archeology.md`) and ping `@creator` with the exact question plus why it is blocking the Action Plan item.
- Do not proceed without that response when the information is blocker-critical.

## 8. Document Maintenance
- Whenever you change process, update both this Runbook and `docs/AGENT_PROMPTS.md`.
- Keep `docs/action_plan.md` and `docs/archeology.md` synchronized with reality—these are the canonical progress trackers.

Following this runbook is part of the review checklist; deviations require maintainer approval.
