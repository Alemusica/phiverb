# Wayverb Apple Silicon Migration Log

This file tracks iterative progress while porting and validating Wayverb on Apple Silicon Macs. New entries should be appended to the top so the latest work is easiest to find.

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
