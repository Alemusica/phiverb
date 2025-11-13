# Spatial Audio Framework Modernization Plan

Legacy reference: `Spatial_Audio_Framework/` (Reuben Thomas, 2017). Usalo come mappa per la
pipeline originale (renderer -> WAV -> binaural decoder). Lo scopo di questo piano è
aggiornare il decoder binaurale e il toolchain correlato allo stato dell'arte 2025.

## Current Architecture
- **Emissione**: waveguide + raytracer producono IR multibanda e li passano a
  `bin/render_binaural` / `Spatial_Audio_Framework` per la convoluzione binaurale.
- **Decoder legacy**: HRTF offline, runtime single-thread, nessun supporto FOA/HOA
  moderno né pipeline streaming.
- **Interfacce**: `src/waveguide/postprocess`, `src/raytracer/postprocess`, `bin/render_binaural`
  consumano le stesse API, quindi la nuova implementazione deve preservare queste firme.

## 2025 Goals
1. Integrare un decoder binaurale moderno (es. ultima release dello Spatial Audio Framework o
   alternativa equivalente) con supporto per FOA/HOA e preset HRTF aggiornati.
2. Mantenere compatibilità con i moduli in ristrutturazione (waveguide, raytracer): l’output
   dei due solver deve essere compatibile con il nuovo decoder senza conversioni manuali.
3. Fornire benchmark/QA (ILD/ITD, T20/T30) e log standard come previsto dalla Runbook.

## Workstreams
1. **Research & Design (branch `audio/spatial-plan-001`)**
   - Analizza `Spatial_Audio_Framework/` + note di Reuben per capire i punti di integrazione.
   - Raccogli documentazione dei decoder moderni (chiedi ASK/GPT-Pro se servono articoli).
   - Produce specifica tecnica in questo file (aggiornare con API, formati, dipendenze).
2. **Implementation (branch `audio/spatial-integration-001`)**
   - Integra il decoder scelto in `bin/render_binaural` + `src/waveguide/raytracer postprocess`.
   - Aggiunge build steps (CMake deps, script) e mantiene compatibilità con i solver.
3. **QA & Tooling (branch `qa/spatial-audio-001`)**
   - Aggiorna `scripts/qa/` con test ILD/ITD, T20/T30 e comparazioni vs legacy.
   - Logga tutto in `build/logs/app/` e `build/logs/regressions/` secondo Runbook.

## Evidence
- Ogni PR deve citare le entry del diario (`logs/control_room/<agent>.md`), i log QA, e gli
  aggiornamenti a `docs/action_plan.md` / `docs/archeology.md`.
