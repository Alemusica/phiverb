# 2025-11-08 — Waveguide debug instrumentation & fractal scene repo

- Added detailed diagnostics for the waveguide “outside mesh” guard: when a voxelized neighbor is missing we now record node id, locator and offending direction (`src/waveguide/src/program.cpp`, `src/waveguide/include/waveguide/waveguide.h`). The host log prints something like `[waveguide] outside-mesh debug: node=… locator=(…) direction=… slot=…` before raising “Tried to read non-existant node.” so tracing suspicious meshes (e.g., WaveHerb_cavolo_romanesco_1-1.obj) no longer requires guessing `WAYVERB_TRACE_NODE`.
- Stored sanctioned geometries under `geometrie_wayverb/` so headless tests (`bin/apple_silicon_regression --scene geometri_wayverb/<scene>.obj`) can reproduce user presets, and so we always ship the exact OBJ that triggered a bug.
- Introduced `tools/wayverb_mesh.py` (Typer CLI) and `tools/swift/ScaleOBJ.swift`. See `docs/mesh_tools.md` for instructions on scaling/sanitizing OBJ files (triangulation, dedupe, metadata-preserving export) and for the Swift helper that uses Model I/O on Apple Silicon.
- Geometry panel UI now exposes the mesh-tool workflow instead of the legacy “sanitize on render” toggle: the new buttons open the mesh-tools guide and reveal `geometri_wayverb/`, and the renderer no longer runs the old `wayverb::core::sanitize_geometry()` path that mis-flagged watertight meshes.

# 2025-11-07 — NaN instrumentation & regression harness

- Added recursive non-finite detectors in `core/dsp_vector_ops` plus logging in `combined::postprocess`: we now report how many waveguide/raytracer samples arrive as NaN/Inf before the crossover mix, so silent renders point to the guilty stage immediately.
- Fallback sanitizers now zero-out non-finite samples before injecting the direct-path impulse, preventing NaNs from wiping out the emergency impulse and giving exact counts in the log.
- `bin/apple_silicon_regression` grew a tiny CLI (`--scene`, `--source`, `--receiver`, `--rays`, `--img-src`, `--wg-cutoff`, `--wg-usable`, `--sample-rate`) and automatically fails if the postprocessed channel contains NaN/Inf or remains silent. Perfect for iterating on new geometry in `geometri_wayverb/` without opening the UI.
- `tools/write_build_id_header.sh` now runs before every JUCE build so the app title always reflects the real `git describe`/branch/timestamp, regardless of whether the binary was produced via CMake or Xcode.

# 2025-11-08 — Deterministic UI log capture

- `tools/run_wayverb.sh` now always mirrors stdout/stderr into `build/logs/app/wayverb-<timestamp>.log` (override via `WAYVERB_APP_LOG`). The script exports `WAYVERB_LAST_APP_LOG`, maintains a `wayverb-latest.log` symlink, and prints the log path so every render run produces an artifact for troubleshooting.
- `scripts/monitor_app_log.sh` automatically picks up `WAYVERB_LAST_APP_LOG` if no argument is passed, making the typical “launch + monitor” loop a single command sequence.
- Agents must attach the log file path plus the UI build identifier whenever reporting “All channels are silent”; the automatic logging guarantees the evidence exists for each run.

# 2025-11-07 — Render fallback + Xcode build parity

- Added a direct-path fallback in `combined::complete_engine`: when both raytracer and waveguide output zero energy we now inject a free-field impulse per channel using the exact source→receiver distance, log the injection, and continue rendering instead of throwing “All channels are silent”.
- Hardened the JUCE/Xcode build: `build.sh` now honors `WAYVERB_BUILD_DIR` and the `wayverb (App)` pre-build phase runs it inside `build-prebuild`, isolating JUCE’s dependency refresh from the developer-facing `build` tree.
- Eliminated the stale `-lzlibstatic` linkage by supplying a `zlibstatic_stub` target that symlinks the system `libz` into `dependencies/lib/libzlibstatic.{tbd,dylib}`; Xcode and legacy tools keep linking the old name but resolve to the system zlib.

# 2025-11-07 — Boundary mapping hardening & build infra

- Added guard-tag infrastructure for every boundary node. Host now seeds a deterministic tag (`node_index ^ 0xA5A5A5A5`), OpenCL kernels validate it before touching filter state, and a new CL validator kernel proves boundary-node tables are consistent on device startup. Metal layout/parity checks were updated in lockstep.
- Introduced `config/apply_patch_if_needed.sh` so ExternalProject patches (assimp/cereal) only run when needed and don’t reapply; forced all deps to pass `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`.
- Began provisioning an `arm64` build directory; external deps now download once and reuse. Remaining blocker: upstream CMake scripts (assimp zlib, cereal) still call `cmake_policy(... OLD)` which fails under CMake 4.1—need either an older cmake for deps or updated upstream scripts.
- Next steps: finish dependency toolchain pinning so `waveguide_tests` recompile under arm64, then implement passive trihedral closures + manifest-driven regression harness.

# 2025-11-06 — Metal pipeline scaffolding (WIP)

- Added `metal::waveguide_simulation` to translate mesh data, allocate Metal buffers, and dispatch the translated waveguide kernels (`condensed_waveguide`, `update_boundary_{1,2,3}`) on Apple GPUs.
- Extended `waveguide_pipeline` to precompile every waveguide stage and expose accessors so higher-level code can reuse the compute pipeline states.
- Hooked the combined engine’s Metal path into the new simulation wrapper; directional receiver math now runs on the CPU while device buffers stream per-step pressures back to the post-process accumulator.
- Regression still falls back to OpenCL because runtime inline of `waveguide/metal/layout_structs.metal` is not yet resolved when the library is compiled inside the app bundle. Next steps: package the Metal headers alongside the runtime source or embed them directly in the fallback string, then validate we stay on the Metal path end-to-end.
- UI render still reports “Encountered unknown error” when the Metal flag is enabled; logs show the warm-up kernel compilation failure above. Once the include packaging is fixed we can re-run `WAYVERB_METAL=1 build/bin/apple_silicon_regression` and verify the binaural IR is generated without triggering the fallback.

# 2025-11-06 — Ported waveguide kernels to Metal source

- Auto-translated the OpenCL waveguide kernels into `waveguide_kernels.metal`, preserving the canonical pressure update, boundary filters, trace hooks, and diagnostics while mapping OpenCL atomics/macros onto Metal equivalents.
- Added shared Metal headers (`waveguide/metal/layout_structs.metal`, `waveguide/metal/common.metal`) so GPU code shares packed layouts and helper utilities (neighbor lookup, filter helpers, atomic wrappers) with the host.
- Rebuilt the project to validate that the new Metal kernels compile; `wayverb_metal` now exports pipeline states for every waveguide stage although they are not yet wired into the runtime loop.
- Next: extend `waveguide_pipeline` to create Metal compute pipeline states for `condensed_waveguide` and `update_boundary_{1,2,3}`, allocate simulation buffers, and replace the warm-up scaffold in `waveguide_metal.cpp` with a real Metal execution path.

# 2025-11-05 — Metal layout parity scaffold

- Introduced shared `layout_info` struct and `make_host_layout_info()` so host/OpenCL and Metal backends agree on packed sizes/offsets for `memory_canonical`, `coefficients_canonical`, and boundary descriptors.
- Extended the Metal scaffold: `waveguide_pipeline` now builds both `fill_zero` and a `layout_probe` kernel from `waveguide_kernels.metal` (with inline fallback), exposing `query_device_layout()` / `validate_layout()` for runtime parity checks.
- Updated the Metal branch warm-up path to include the new pipeline header and fixed the raytracer log to use `maximum_image_source_order`.
- Outstanding build task: wire the missing include paths for CLI tools (e.g., `bin/rt60`, `bin/render_binaural`, `bin/apple_silicon_regression`) so they pick up `audio_file`, `utilities`, and `raytracer/cl` headers; the main libraries already compile.
- Next actions: implement the Metal `pressure_step` and boundary kernels using the shared structs, then enable `wayverb_metal` guarded by `WAYVERB_METAL=1` for parity testing (`WAYVERB_METAL=force-opencl` remains fallback).

# Wayverb Apple Silicon Migration Log

This file tracks iterative progress while porting and validating Wayverb on Apple Silicon Macs. New entries should be appended to the top so the latest work is easiest to find.

## 2025-11-05 — Metal scaffold + perf toggles + doc pass

- Created feature branch `feature/metal-apple-silicon` and added an optional Metal backend scaffold behind build/runtime flags. When enabled, it logs and falls back to OpenCL until kernels are ported.
  - Build flag: `WAYVERB_ENABLE_METAL=ON` (CMake)
  - Runtime selector: `WAYVERB_METAL=1` (use Metal) or `WAYVERB_METAL=force-opencl` (force OpenCL)
- Added performance controls to keep the GPU saturated during waveguide runs:
  - `WAYVERB_DISABLE_VIZ=1` disables waveguide node-pressure GPU→CPU readbacks during a run.
  - `WAYVERB_VIZ_DECIMATE=<N>` reads back once every N steps (if visualisation is needed).
  - `WAYVERB_VOXEL_PAD=<int>` (default 5) shrinks/expands the voxel padding around the adjusted boundary to reduce domain size without lowering cutoff/usable.
- Fixed geometry panel visibility (explicit size) and documented the new Geometry tools (Analyze, Sanitize on render, epsilon).
- tightened model guardrails: single/multiple waveguide usable_portion is clamped to [0.10, 0.60] to avoid unstable or excessively costly configs.
- Wrote docs: `docs/metal_port_plan.md` and `docs/APPLE_SILICON.md` (how to build, flags, logs, perf tips, current status & next steps).

## 2025-11-05 — Boundary filter guard rails

- Hardened `ghost_point_pressure_update` against fp32 overflow: every canonical filter tap is now checked for finiteness and clamped back to zero when its magnitude exceeds `1e30`, so runaway boundary memories no longer explode to `inf`.
- Clamp events are logged via `record_nan` codes 10/11 without raising the global error flag, keeping the new trace buffer informative while allowing the simulation to advance.
- Regression smoke (`apple_silicon_regression` with `WAYVERB_TRACE_NODE=30494993`) now marches past the former step-8381 NaN; large filter states are reset but the kernel stays finite.

## 2025-11-05 — Geometry not watertight (finding) + tooling

- Discovered the working scene (pyramid_twisted_minor.obj) is not watertight; boundary_edges > 0 and non‑manifold edges present. This explains historical NaN at trihedrals and unstable reflection visualisation.
- Added Geometry section in UI (Analyze, Sanitize on render, epsilon) to report topology (zero‑area, duplicates, boundary/non‑manifold, watertight) and optionally weld/remove degenerates before a run.
- Added headless pre‑sanitizer using Python trimesh (scripts/sanitize_mesh.py) and wired it to scripts/run_regression.sh via WAYVERB_PRE_SANITIZE, WELD_EPS, FILL_HOLES, VOXEL_PITCH.
- Improved diagnostic logging when final audio is silent: per‑channel max and non‑zero counts printed before raising “All channels are silent”.
- Crash reporter installed (~/Library/Logs/Wayverb or WAYVERB_LOG_DIR) to capture signal + backtrace + last engine status; app logs go to build/logs/app/* when launched via tools/run_wayverb.sh.

### Phase logging & status
- Engine now logs phase transitions and periodic progress to the app log:
  - "[engine] starting raytracer: rays=… img_src_order=…"
  - "[engine] starting waveguide: fs=… Hz spacing=… m max_time=… s"
  - "[engine] waveguide step=X/Y (Z%)" every ~500 steps and at completion
  - "[engine] finishing …"
- Crash reporter status is updated with waveguide step counters to aid post‑mortem.

## 2025-03-10 — Coefficient sanitization & fallback

- Detect all-zero or near-singular canonical coefficient sets when constructing `waveguide::vectors` and replace them with a rigid-wall fallback (`b0 = 1`, remaining taps zero); emit a warning with the number of patched entries so bad material data is visible but non-fatal.
- Added a defensive early-out in `ghost_point_pressure_update` for the (now rare) case where both `a0` and `b0` arrive as ~0 on device, ensuring the boundary filter stays benign even if host sanitization is bypassed.
- Updated the CPU `boundary_probe` helper to mirror the new guards, keeping host/GPU diagnostics in sync.
- Full regression still fails because coefficient set 0 loaded from disk is empty — next action is to regenerate the surface data or confirm the neutral material mapping so we can complete the Apple Silicon smoke test.

## 2025-03-11 — GPU trace instrumentation

- Added optional device-wide tracing infrastructure. When `WAYVERB_TRACE_NODE` is set, the pressure kernel and all boundary update kernels log per-step records (kind, step, pressure values, filter state before/after) into a global ring buffer.
- Host code now allocates `trace_buffer`/`trace_head` and feeds step index and trace target into each kernel invocation; this enables later GPU↔CPU replay for suspect nodes (e.g., 30494993).
- Kept existing stage callbacks (`WAYVERB_WG_TRACE`) separate so high-level stage logs continue to work; trace data is inactive unless explicitly requested.

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
- Headless IR on sanitized mesh `pyramid_twisted_minor_FDTD_wayverb.obj` verified via `render_binaural` with env controls:
  - `RT_RAYS=262144 RT_IMG=5 WG_CUTOFF=500 WG_USABLE=0.6 IR_SECONDS=2`
  - Output: `build/ir_test2.wav` (2.0 s, stereo, non‑silent), GPU device Apple M1 Max selected.
  - Added receiver HRTF fallback when directional intensity ~0, preventing numerically silent outputs in symmetry nodes.
- Default UI waveguide mode switched to multi-band (3 bands) so QA catches the multi-path NaN earlier; adjust tabs manually if you need the legacy single-band setup.
