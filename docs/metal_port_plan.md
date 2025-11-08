# Apple Silicon (Metal) Port Plan

Goals
- Run core simulation stages natively on Apple Silicon GPUs using Metal.
- Keep OpenCL backend as a fallback and for non‑macOS targets.
- Preserve numerical parity and add profiling to prove speedups.

Scope (phases)
- Phase A: Waveguide kernels → Metal compute pipeline
  - Map existing buffer layouts (nodes, boundary indices, histories) 1:1.
  - Translate `node_inside`, `node_boundary`, `boundary_*` kernels to MSL.
  - Use private MTLHeaps and triple‑buffering for histories to avoid stalls.
  - Command graph: encode per‑stage into separate command buffers; overlap where safe.
- Phase B: Raytracer intersections → Metal
  - Keep BVH build on CPU initially; batch rays/intersections on GPU.
  - Structure‑of‑arrays for triangles; 256‑thread threadgroups; subgroup reduction.
- Phase C: Post‑processing → Accelerate/vDSP
  - FIR/FFT, HRTF convolution and mixdown via vDSP/Accelerate.
- Phase D: Instrumentation
  - os_signpost + MTLCapture for GPU timeline; CSV export of per‑kernel timings.

Design
- New optional module `metal` (Objective‑C++), gated by `WAYVERB_ENABLE_METAL`.
- Runtime selection:
  - If `WAYVERB_ENABLE_METAL=1` and `MTLDevice` available, prefer Metal; else OpenCL.
  - Env knob `WAYVERB_METAL` to force/disable.

Incremental adoption
- Start by mirroring the OpenCL mesh build path (node classification & boundary indexing) as Metal compute. Once stable, swap the waveguide pressure/boundary update loop.
- Maintain identical buffer packing to reuse host‑side code and minimize surface area.

Validation
- Unit tests for per‑kernel parity on small meshes.
- Golden outputs for short runs (<= 64 steps) compared across backends.

Risks & mitigations
- Numerical drift from different math libs: compare at tolerances; clamp guards remain in place.
- Xcode integration: keep ObjC++ isolated; CMake builds gated OFF by default to avoid breaking non‑macOS.

Deliverables (initial)
- `metal_context` wrapper (MTLDevice, MTLCommandQueue) + stubs.
- Build system option (OFF by default).
- Logging hooks for GPU profiling.

