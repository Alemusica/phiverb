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
