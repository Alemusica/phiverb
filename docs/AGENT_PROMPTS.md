# Agent Prompts / Hand-off Notes

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

## Agent Ops / Token Guard

- Runner macOS/Metal: quando il self-hosted `self-hosted, macos, metal` è attivo, conferma nel log della PR con `runner=macos-metal ready` (fallisce la CI se manca il label, quindi tienilo monitorato).
- Controlla sempre il budget di token prima di dumpare output lunghi: `scripts/agents/token_guard.sh path/al/file.md` oppure `... | scripts/agents/token_guard.sh`.
- Regole di checkpoint:
  1. 20%: apri una PR in draft (anche vuota).
  2. 60%: esegui `scripts/agents/checkpoint_or_pr.sh` per salvare lo stato.
  3. 80%: la PR deve esistere e va aggiornata con l’ID dell’action plan.
  4. 90%: nuovo checkpoint + esci; se servono info, usa `scripts/ask/...`.
