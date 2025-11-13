# Agent Prompts / Hand-off Notes

> Tutti gli agenti devono leggere e seguire la Runbook (`docs/agent_runbook.md`). Le sezioni sotto aggiungono solo specifiche tecniche.
>
> **Uso della documentazione legacy:** i file storici (es. note originali di Reuben) vanno trattati come “mappe” dell’edificio: servono solo per capire cosa è stato fatto e dove intervenire, ma non per ripristinare pipeline o test obsoleti. Ogni implementazione deve rifarsi alle sezioni 2025 (Runbook + Action Plan); i test legacy si consultano solo per archeologia o confronto, mai come riferimento operativo.

## Raytracer (worktree `PhiVerb-rt`, branch `rt/...`)

- Prima di continuare, apri `docs_source/boundary.md` e leggi il blocco
  **"Geometric Implementation → 2025 status / design targets"**.
- Tutto il codice RT deve rispettare quei requisiti moderni: BRDF
  energy-preserving (`f·cos/pdf`), diffuse rain obbligatorio, parity ISM↔PT.
- I paragrafi storici nello stesso file sono solo background.
- Cita quel blocco (con percorso e commit) nelle PR/ASK quando serve.

## DWM (worktree `PhiVerb-dwm`, branch `dwm/...`)

- Riferimento principale: `docs_source/boundary.md`, sezione
  **"DWM Implementation → 2025 status / design targets"**.
- Usa solo sorgenti transparent/PCS, boundary SDF+DIF, guard-tag + fail-fast,
  layout SoA/Morton. Evita "closest triangle" a runtime.
- Annota nelle PR che il lavoro segue quella sezione aggiornata.

Mantieni questo file aggiornato se cambiano le direttive operative.

## Metal Assistant Agent

- Custom agent disponibile: `.github/agents/phiverb-metal-assistant.agent.md`
- Specializzato in: Metal API porting, Apple Silicon optimization, debugging audio rendering
- Script di supporto:
  - `scripts/test_metal.sh` - Test compilazione shader Metal e unit tests
  - `scripts/validate_audio.py` - Validazione output audio
  - `scripts/benchmark.sh` - Benchmark performance
- CI dedicata: `.github/workflows/metal-ci.yml`
- Per invocare l'agente: usa il file agent con GitHub CLI o Copilot CLI
- Esempio: `gh copilot chat -a .github/agents/phiverb-metal-assistant.agent.md "How do I fix clBuildProgram error?"`

## Agent Ops / Token Guard (riassunto Runbook)

- Segui la Runbook per worktree obbligatori, log (`tools/run_wayverb.sh` + `scripts/monitor_app_log.sh`), regressioni (`tools/run_regression_suite.sh`) e gestione mesh `geometrie_wayverb/`.
- Runner macOS/Metal: quando il self-hosted `self-hosted, macos, metal` è attivo, commenta `runner=macos-metal ready` sulla PR (la CI fallisce se manca).
- Dev diary: logga ogni sessione con `scripts/agents/log_note.sh "messaggio"` (file `logs/control_room/<agent>.md`) e referenzia quelle note in PR/action plan. Quando completi una voce dell’Action Plan, spunta la checkbox in `docs/action_plan.md` e cita log/commit.
- Policy di debug: dopo 5 tentativi/fallimenti consecutivi dello stesso test devi fermarti e aprire un’ASK (via `docs/archeology.md`/Control Room) con log e note; riprendi solo dopo feedback.
- Token guard: prima di output >1k token usa `scripts/agents/token_guard.sh ...`; se entra in stato giallo/rosso rispetta i checkpoint descritti nella Runbook.
- Checkpoint obbligatori (20/60/80/90%) e riferimenti ai log/action plan sono verificati via template PR; vedi `docs/agent_runbook.md` §3-5.
