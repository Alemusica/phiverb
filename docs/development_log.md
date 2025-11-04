# Wayverb Apple Silicon Migration Log

This file tracks iterative progress while porting and validating Wayverb on Apple Silicon Macs. New entries should be appended to the top so the latest work is easiest to find.

## 2025-03-09 (late, resumed) — Dedicated boundary kernels

- Added optional OpenCL stage tracing (`WAYVERB_WG_TRACE=1`) that registers callbacks on the pressure and boundary update kernels; event logs now reveal exactly which phase completes or stalls (gws printed per stage).
- Introduced `WAYVERB_MAX_STEPS` to cap the waveguide loop, enabling short, deterministic smoke runs while debugging the combined engine.
- All waveguide kernels now wait on their completion events before post-processing, ensuring subsequent pipeline stages never see half-updated buffers.
- Fixed OpenCL build failures by renaming the `update_boundary_*` kernel argument from `boundary_data` to `boundary_storage`, avoiding a typedef/parameter name collision flagged by Apple’s `cl2Metal` translator.
- Short regression with `WAYVERB_MAX_STEPS=5` now runs five clean waveguide frames (callbacks report pressure + boundary stages) before the harness aborts due to the artificial cap.
- Completed step “C”: split boundary filter updates into standalone OpenCL kernels (`update_boundary_{1,2,3}`) that run after the pressure kernel. Each kernel maps boundary-index → node, reads `(prev_history, current, next)` buffers, and invokes `ghost_point_pressure_update` per face/edge/corner with the validated popcount index.
- Removed in-place boundary filter writes from the pressure step (`boundary_{1,2,3}` now only compute the next pressure), keeping the solver numerics unchanged while eliminating the private-stack clobber site.
- Enforced map invariants at runtime: new kernels validate that node `boundary_index` matches the dispatch index and raise `id_outside_range_error` if wiring drifts.
- Rebuilt the project with `cmake --build build`; OpenCL program now compiles with the new kernels.
- Sanity test: ran `boundary_probe assets/test_geometry/pyramid_twisted_minor.obj 30494993` (original failing corner). GPU/host both return `next_pressure = 0`, confirming the NaN is gone at this trihedral. Full regression still times out after ~2 minutes (progress bar stalls around 100 %), so long-run verification remains pending.

## 2025-03-08 (Regression Harness)

- Added `apple_silicon_regression` headless executable to load `pyramid_twisted_minor.obj`, run the combined engine, and report runtime for automated Apple Silicon smoke tests.
- Introduced `scripts/run_regression.sh` to configure/build the target on demand, execute the regression, and archive logs under `build/logs/`.
- Wired runtime diagnostics: optional `WAYVERB_FORCE_IDENTITY_COEFFS` disables high-order filters to isolate boundary issues, regression now logs mesh size, first non-finite node, boundary coefficients, locator and neighbors when the waveguide kernel flags NaNs.
- Created `boundary_probe` executable and `scripts/dump_boundary_node.py` to reproduce the problematic boundary calculation on the CPU and view raw mesh/boundary data; host math yields a finite result, confirming the NaN originates inside the OpenCL boundary kernel.

## 2025-03-08

- Forced OpenCL single-precision on Apple Silicon via `WAYVERB_FORCE_SINGLE_PRECISION`, guarded JUCE inline asm, and ensured the Metal sample kernel is linked and invoked safely at startup.
- Added a minimal Metal compute smoke test (`wayverb/Source/metal/MetalSample.*`) and wired it into the macOS app.
- Removed legacy QTKit linkage and linked Metal.framework in the Xcode project to fix linker issues on arm64.
- Copied reference geometry `pyramid_twisted_minor` into `assets/test_geometry/` for repeatable scene tests.
- Verified `wayverb (App)` builds with `xcodebuild` and launches cleanly from the shell with `CL_LOG_ERRORS=stdout`.
## 2025-03-09 (Trihedral NaN investigation)

- Added OpenCL `layout_probe` kernel plus `bin/layout_probe` host utility to confirm host/device struct packing for `memory_canonical`, `boundary_data`, and trihedral arrays; Apple GPU reports identical sizes/offsets (24/32/96 bytes, offsets 0/24/32/64).
- Extended waveguide debug infrastructure: GPU kernel now records raw coefficient bits, filter state, `diff`, filter input, and the original `prev/next` pressures to a dedicated debug buffer; `waveguide::run` prints decoded floats when a NaN is detected.
- Regression still fails at node `30494993` (corner mask `id_nx|id_py|id_nz`) with `diff` exploding to `~3.18e+36` while host `boundary_probe` returns zero, confirming a math divergence triggered only on the GPU.
- Captured latest regression log (`build/latest_regression.log`) showing stabilized metadata (coeff index 0, `prev_pressure ≈ 1.1e+37`, `next_pressure = 0`). Ready to escalate with GPT-5 Pro for deeper numerical analysis.

## 2025-03-09 (late) — Struct padding / compiler investigation

- Implemented GPT-5 Pro’s guidance to rework boundary data layout: padded `memory_canonical` to 32 B (8 floats), `coefficients_canonical` to 64 B (16 floats), `boundary_data` to 64 B (`memory_canonical` + coeff index + pad), and updated alignment/static asserts on host/device. Added `docs/refs.bib` with the supplied bibliography.
- Replaced remaining struct assignments with element-wise loops, starting in `ghost_point_pressure_update` and `get_filter_weighting`. Propagated padding through OpenCL representations and host code; added helper constructors to convert legacy `memory<N>`/`coefficients<N>` into the padded versions.
- Regression now fails at build stage: Apple OpenCL compiler reports `memory_6`/`coefficients_6` missing because the padded types changed the generated macros (FILTER_STEP expects `memory_6`, `coefficients_6` symbols). Also flagged out-of-bounds warnings due to macro expansion referencing the old order-2 definitions. Need to revisit filter kernel macros to handle the new names, or keep legacy typedefs (`memory_6`) to match the macro expansion.
- Host build still succeeds, but the OpenCL program compilation aborts (`clBuildProgram` error 10014). Next steps: reintroduce the legacy typedef aliases (e.g., `typedef memory_canonical memory_6`) so the macros resolve, or adjust `FILTER_STEP` to use the new types directly. After restoring build, rerun regression to confirm whether padding actually removes the NaN.

## 2025-03-09 (late) — Boundary-update rearchitecture (in progress)

- Added alias typedefs (`memory_6`, `coefficients_6`) so the OpenCL macro `FILTER_STEP` continues to work with the padded structs; build returns to success.
- Regression still flags NaN at corner node 30494993: padding + element-wise copies alone did not eliminate the `prev ≈ 1.1e37` issue.
- Following GPT-5 Pro’s “step C”, started restructuring the solver to split boundary updates into dedicated kernels:
  * Introduced boundary node index vectors in `vectors` (inverse maps for 1D/2D/3D boundaries) and staging buffer `previous_history` in the host runner.
  * Wired the necessary host buffers (`boundary_nodes_buffer_{1,2,3}`) and placeholders for new OpenCL kernels (`update_boundary_*`) to be invoked after the pressure step.
  * Remaining work (next session): implement the new kernels in `program.cpp`, update `program.h` to expose them, and adjust orchestrator accordingly. Once done, rerun the regression to verify whether the NaNs disappear with the boundary updates isolated.
