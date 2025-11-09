# Wayverb Archeology Action Plan

Questo file vive nel repo e rimane il riferimento unico per tenere traccia
dell'avanzamento. Consultalo insieme alla [Runbook](agent_runbook.md): quando completi una voce devi aggiornare entrambi dove pertinente. Ogni blocco ha:

- **Descrizione** e **path** dei file coinvolti.
- **Checklist** con criteri di accettazione.
- **Verifiche** da segnare quando i test passano (aggiungere `✅` accanto al punto completato o un commento datato).

> **Regola:** quando completi una voce, aggiorna questo file nella stessa PR.

---

## Scope agenti

- **Agente Raytracing** — gestisce il cluster 1 (Raytracer). Lavoro completato, nessuna azione richiesta per il flusso DWM.
- **Agente DWM (questo)** — si concentra sui punti marcati `[DWM]` all’interno del cluster 2 (Waveguide).
- **Fuori scope attuale** — Cluster 3‑6 (Metal backend, Materiali/UI, QA & Tooling, Spatial Audio Framework) restano assegnati ad altri agenti.

---

## 1. Raytracer — energia e diffuse rain

_Responsabile: agente Raytracing; fuori scope per il flusso DWM._

**File principali**
- `src/raytracer/include/raytracer/pressure.h`
- `src/raytracer/src/cl/brdf.cpp`
- `src/combined/include/combined/postprocess.h`

**Checklist**
- [x] Disegnare l’interfaccia `PathEnumerator` (ISM e RT) e spostare il calcolo energetico su questo livello.
  - Implementato `image_source::enumerate_valid_paths` con `path_event_view` (`src/raytracer/include/raytracer/image_source/path_enumerator.h`) e aggiornato `postprocess_branches` a usare il nuovo callback.
- [x] Implementare BRDF speculare+diffusa energicamente conservativa (Lambert 1/π, throughput `f·cos/pdf`).
  - `src/raytracer/src/program.cpp` ora campiona i raggi diffusi con distribuzione cosine-weighted e annota `scatter_probability`/`sampled_diffuse`.
  - `src/raytracer/src/stochastic/program.cpp` aggiorna il throughput dividendo per le probabilità di selezione dei lobi.
- [x] RNG deterministico + gating dei contributi diffusi (`simulation_parameters::rng_seed`, `reflector` seeded, `stochastic` kernel usa `sampled_diffuse`).
  - _Progress:_ l’energia in uscita lungo il cammino speculare ora viene pesata con `(1 - scattering)` (`src/raytracer/src/stochastic/program.cpp`).
- [x] Validare il decadimento: `raytracer/tests/reverb_tests.cpp` calcola l’EDC di uno shoebox e verifica che `T30` cada tra Sabine/Eyring e superi i −60 dB.
- [ ] Aggiungere “Diffuse Rain” deterministico (proiezione dei contributi diffusi ai ricevitori visibili).
- [ ] Validare early reflections: ISM vs RT entro ±1 sample e ±0.5 dB sugli ordini coperti.

**Verifiche**
- Test di comparazione ISM↔RT (shoebox).
- Script di controllo T20/T30 per verificare che l’IR abbia coda fisicamente plausibile.

---

## 2. Waveguide — sorgente trasparente e boundary stabili

**File principali**
- `src/waveguide/include/waveguide/pcs.h`
- `src/waveguide/src/boundary_coefficient_program.cpp`
- `src/waveguide/include/waveguide/canonical.h`
- Kernel Metal/OpenCL corrispondenti.

**Checklist** _(solo gli elementi `[DWM]` sono in carico a questo agente)_
- [ ] Sostituire la sorgente attuale con PCS/transparent source (nessuna growth, nessuna riflessione al nodo).
- [x] Precomputare SDF/normali e caricare materiali→DIF (niente più ricerche “closest triangle” a runtime).
  - `scripts/waveguide_precompute.py` emette ora `*.sdf.json` + `*.{sdf,normals,labels}.bin` e `*.dif.json`; il runtime (`precomputed_inputs`) li carica per popolare `boundary_index_data` senza kernel di ricerca.
- [ ] [DWM] Separare kernel interior/boundary (SoA/Morton order) e aggiungere guard numerici (isfinite, clamp).
- [ ] Verificare passività: energia costante su pareti rigide, decrescimento monotono con α>0.
- [ ] [DWM] Logs diagnostici: contatori NaN/Inf=0, abort immediato se >0.

**Verifiche**
- Test automatico che verifica la condizione CFL e la passività.
- Harness che gira i kernel in modalità “diagnostic” (wayverb_allow_silent_fallback=0) e fallisce al primo NaN.

---

## 3. Metal backend — parità con OpenCL

**File principali**
- `src/combined/src/waveguide_metal.cpp`
- `src/metal/include/*`, `src/metal/src/*`
- `tools/run_regression_suite.sh`

**Checklist**
- [ ] Progress callbacks (`MTLSharedEvent`/`addCompletedHandler`) e logging errori GPU → abort immediato.
- [ ] `mathMode=.precise` (no fast-math) e RNG deterministico identico a OpenCL.
- [ ] Layout unificato (`packed_float3` dove serve, `static_assert` su host/shader).
- [ ] Threadgroup dimension multipla di `threadExecutionWidth`.
- [ ] Regression harness che confronta (bitwise corto, RMS lungo) OpenCL vs Metal.

**Verifiche**
- Script `tools/run_regression_suite.sh` lancia entrambe le pipeline e fallisce se gli IR divergono oltre la soglia richiesta.
- Log che attestano progressi periodici (≥1 update/s per scene pesanti).

---

## 4. Materiali / Output / UI

**File principali**
- `src/combined/src/model/presets/material.cpp`
- `wayverb/Source/*` (output path, help panel, visualizzazioni)
- `docs/APPLE_SILICON.md`, `docs/archeology.md`

**Checklist**
- [ ] Integrare dataset PTB/openMat (α,s per banda) e aggiornare i preset.
- [ ] Migrare gli output path a `std::filesystem` (creazione dir, path portabili).
- [ ] Implementare export FOA (ACN/SN3D) da p,u; validare con tool IEM/SPARTA.
- [ ] Completare help/tooltips con i nuovi workflow e le policy fail-fast.
- [ ] Aggiornare `docs/archeology.md` con ogni avanzamento significativo.

**Verifiche**
- Test di decode FOA (onda piana → errore direzione <3°).
- Esecuzioni di listening tasks e log associati (da allegare nelle PR).

---

## 5. QA & Tooling

**File principali**
- `analysis/comment_inventory.json`
- `analysis/todo_comment_xref.md`
- `scripts/monitor_app_log.sh`
- `tools/comment_inventory_diff.py`
- `tools/todo_comment_xref.py`
- `tools/run_regression_suite.sh`
- `docs/archeology.md`

**Checklist**
- [ ] Aggiornare inventario e xref ogni volta che si tocca `todo.md` o i commenti (script già presenti).
- [ ] Usare sempre `scripts/monitor_app_log.sh` per catturare `[combined]` stats e “All channels are silent”.
- [ ] Eseguire `tools/run_regression_suite.sh` con `WAYVERB_ALLOW_SILENT_FALLBACK=0` prima di dichiarare risolto un bug.
- [ ] Mantenere `docs/archeology.md` sincronizzato (cosa è stato fatto, cosa resta, link ai log).
- [x] `scripts/qa/run_validation_suite.py` applica uno slack assoluto (0.01 s) sui bound Sabine/Eyring per assorbire il rumore T20/T30 e salva i log sotto `build/logs/app/validation-*.log`.
- [x] Stub CLI (`bin/wayverb_cli`) per generare IR sintetici e alimentare la QA finché la pipeline fisica non è pronta.

**Verifiche**
- CI/fuzzing: integrare libFuzzer/AFL++ e RapidCheck come da piano; loggare i risultati in PR.

---

## 6. Spatial Audio Framework / Binaural Decoding

**File principali**
- `docs/audio_spatial_framework_plan.md`
- `bin/render_binaural`, `Spatial_Audio_Framework/*`
- `src/waveguide/postprocess`, `src/raytracer/postprocess`

**Checklist**
- [ ] Aggiornare il piano (docs/audio_spatial_framework_plan.md) con API, dipendenze e risultati della ricerca (eventuale ASK/GPT-Pro documentato).
- [ ] Integrare il decoder aggiornato mantenendo compatibilità con waveguide/raytracer (formati IR, pipeline FOA/HOA).
- [ ] Aggiornare tooling/QA (`scripts/qa/*`) con test ILD/ITD/Txx e loggare ogni run (`build/logs/...`).

**Verifiche**
- Log binaurali allegati alla PR (validation/regressione con percorsi esatti).
- `docs/archeology.md` e `docs/audio_spatial_framework_plan.md` aggiornati con stato, link ai log e riferimenti alle entry del dev diary.

---

### Come usare questo file
1. Prima di iniziare un task, trova la voce corrispondente e assicurati di rispettare i criteri.
2. Una volta completato, modifica la casella checklist (✅) e annota eventuali dettagli/commit/log.
3. Se la voce richiede dati (log RT/T30, comparazioni, ecc.), allegali alla PR e linkali da qui.
4. Quando compaiono nuove richieste (es. da GPT‑5 Pro), aggiungi nuove voci con questa stessa struttura.

# Action Plan — ID: `AP-<CLUSTER>-NNN` (es. `AP-RT-001`, `AP-DWM-003`)
Legenda **Status**: `OPEN` `IN-PROGRESS` `BLOCKED` `DONE`

## Inbox (da Auto‑XRef)
<!-- BEGIN INBOX -->
- `src/raytracer/include/raytracer/pressure.h:67` //  TODO add an abstraction point for image-source finding
- `src/raytracer/include/raytracer/pressure.h:70` //  TODO analytic signal / hilbert function
- `src/raytracer/src/cl/brdf.cpp:48` //  TODO we might need to adjust the magnitude to correct for not-radiating-in-
- `src/waveguide/include/waveguide/pcs.h:72` /// TODO find correct mass for sphere so that level correction still works
- `src/frequency_domain/include/frequency_domain/filter.h:36` /// TODO this is going to be slow because it will do a whole bunch of
- `src/core/include/core/reverb_time.h:25` //  TODO this is dumb because of the linear search.
- `src/combined/include/combined/model/min_size_vector.h:68` //  TODO could return std::optional iterator depending on whether or not
- `src/combined/src/model/output.cpp:51` //  TODO platform-dependent, Windows path behaviour is different.
- `src/combined/src/model/presets/material.cpp:10` /// TODO scattering coefficients
<!-- END INBOX -->
