# Wayverb Code Archeology

Questo documento raccoglie lo stato corrente del progetto emerso
dell'analisi di `todo.md`, dei commenti inline e dei log recenti.

## Cluster principali

1. **Raytracer / energia**
   - Hook per i percorsi completato (`image_source::path_event_view`).
   - BRDF Lambert + throughput corretto e RNG deterministico (`simulation_parameters::rng_seed`, `reflector`).
   - Tail verificata con `raytracer/tests/reverb_tests.cpp` (EDC vs Sabine/Eyring).
   - **TODO**: integrare “diffuse rain” deterministico e confronto MIS ISM↔RT (shoebox speculare).

2. **Waveguide**
   - PCS con massa placeholder e soft source instabile (`waveguide/pcs.h`).
   - Boundary modelling “unbelievably slow” e ancora basato su ricerche del triangolo più vicino (`waveguide/canonical.h`, `boundary_coefficient_program.cpp`).
   - Dev log 2025‑11 documenta NaN multi-band ai corner e guard-tag in corso d’opera.

3. **Metal backend**
   - `combined/src/waveguide_metal.cpp` esplicita che le progress callbacks non sono ancora implementate.
   - `docs/APPLE_SILICON.md` ricorda che il backend è ancora un scaffold e va mantenuto il fallback OpenCL.

4. **Materiali / UI**
   - Scattering coeff uniformi nei preset (`combined/model/presets/material.cpp`).
   - `main_model.cpp` non crea la directory di output, `main_window.h` non notifica l’applicazione prima di chiudere, `reflections_object.cpp` visualizza energia costante.
   - Sezione TODO “app stuff” e “documentation” coprono help panel, listening tasks e confronto con commit universitario.

## Strumenti a supporto

- `analysis/comment_inventory.json` + `analysis/comment_inventory.md` — inventario completo dei commenti.
- `analysis/comment_summary.md` — filtri per categoria.
- `analysis/todo_comment_xref.md` — collegamento TODO ↔ commenti.
- `tools/comment_inventory_diff.py` — diff fra snapshot dell’inventario.
- `tools/todo_comment_xref.py` — rigenera l’xref.
- `tools/run_regression_suite.sh` — esegue `bin/apple_silicon_regression` su tutte le scene forzando `WAYVERB_ALLOW_SILENT_FALLBACK=0`.
- `bin/wayverb_cli` — CLI sintetica che genera IR (per la QA EDC/Txx finché il solver fisico non è integrato).
- `scripts/monitor_app_log.sh` — monitora i log emettendo timestamp e le nuove metriche `[combined]`.

Usare questi artefatti come reference prima di toccare ciascun cluster e aggiornare il documento quando emergono nuove evidenze.

# Archeology — stato & indizi (living)

**Ultima scansione:** <!--XREF-DATE-->2025-11-13 22:45:13<!--/XREF-DATE-->

## Auto-XRef (TODO/commenti)
<!-- BEGIN AUTO-XREF -->
| Percorso | Esempio |
|---|---|
| `src/raytracer/include/raytracer/pressure.h:67` | //  TODO add an abstraction point for image-source finding |
| `src/raytracer/include/raytracer/pressure.h:70` | //  TODO analytic signal / hilbert function |
| `src/raytracer/src/cl/brdf.cpp:95` | //  TODO we might need to adjust the magnitude to correct for not-radiating-in- |
| `src/waveguide/include/waveguide/pcs.h:72` | /// TODO find correct mass for sphere so that level correction still works |
| `src/frequency_domain/include/frequency_domain/filter.h:36` | /// TODO this is going to be slow because it will do a whole bunch of |
| `src/core/include/core/reverb_time.h:25` | //  TODO this is dumb because of the linear search. |
| `src/combined/include/combined/model/min_size_vector.h:68` | //  TODO could return std::optional iterator depending on whether or not |
| `src/combined/src/model/output.cpp:51` | //  TODO platform-dependent, Windows path behaviour is different. |
| `src/combined/src/model/presets/material.cpp:10` | /// TODO scattering coefficients |
<!-- END AUTO-XREF -->

## Open Questions (ASK)
<!-- BEGIN ASK-LIST -->
_(le richieste Ask‑Then‑Act attive/risolte compaiono qui)_
<!-- END ASK-LIST -->
