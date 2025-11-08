# Wayverb on Apple Silicon — Developer Guide

This document summarizes the current state, how to build and run with logs, performance knobs, and the roadmap for the Metal backend. It is the hand‑off reference for anyone resuming development.

## TL;DR

- Branch for Metal work: `feature/metal-apple-silicon`
- App logs: `build/logs/app/wayverb-*.log` (launch via `tools/run_wayverb.sh`)
- Crash logs: `~/Library/Logs/Wayverb` (or `WAYVERB_LOG_DIR`)
- Geometry tools (UI): Left panel → “geometry” → Analyze / Sanitize
- Performance knobs (OpenCL backend):
  - `WAYVERB_DISABLE_VIZ=1` to stop GPU→CPU readbacks of node pressures
  - `WAYVERB_VIZ_DECIMATE=N` to update the UI every N steps
  - `WAYVERB_VOXEL_PAD=3` to shrink voxel padding around the adjusted boundary
- Waveguide “usable portion” is clamped to `[0.10, 0.60]` for stability/perf.

## Build & Run

### App (Xcode UI)

- Binary path (Debug): `wayverb/Builds/MacOSX/build/Debug/wayverb.app/Contents/MacOS/wayverb`
- Launch with logs:

```
CL_LOG_ERRORS=stdout \
WAYVERB_LOG_DIR="$(pwd)/build/logs/crash" \
tools/run_wayverb.sh > build/logs/app/wayverb-$(date +%Y%m%d-%H%M%S).log 2>&1 &
```

- Monitor filtered diagnostics:

```
scripts/monitor_app_log.sh "$(ls -t build/logs/app/wayverb-*.log | head -1)"
```

### Headless tools

- Sanitize OBJ (no simplification): `bin/sanitize_mesh`
- Binaural render (CLI): `bin/render_binaural`
- Regression smoke (Apple Silicon): `bin/apple_silicon_regression`

Build the tools with CMake:

```
cmake -S . -B build
cmake --build build -j
```

### Metal build (scaffold enabled)

Use the helper script or CMake directly:

```
tools/build_metal.sh
# or
cmake -S . -B build-metal -DWAYVERB_ENABLE_METAL=ON
cmake --build build-metal -j
```

Run with Metal backend selected (currently logs and falls back to OpenCL until kernels land):

```
WAYVERB_METAL=1 build-metal/bin/apple_silicon_regression
```

## Logs & Crash Reporter

- App log (console): `build/logs/app/wayverb-*.log`
- Crash/terminate logs: `~/Library/Logs/Wayverb` (or `WAYVERB_LOG_DIR`)
- The crash reporter records last engine status and a best‑effort backtrace.

## Geometry pipeline

- UI: Left panel → “geometry”
  - Analyze → reports vertices, triangles, zero‑area, duplicates, boundary/non‑manifold edge counts, watertight flag
  - Sanitize on render (weld/remove degenerates) with epsilon (default `1e-6`)
- Code: `src/core/include/core/geometry_analysis.h`, `src/core/src/geometry_analysis.cpp`
- CLI: `bin/sanitize_mesh`

## Performance on Apple Silicon (OpenCL backend)

Waveguide dominates runtime (millions of nodes × thousands of steps). Keep quality but control overhead:

- Disable or decimate visualization readbacks (GPU→CPU):
  - `WAYVERB_DISABLE_VIZ=1` or `WAYVERB_VIZ_DECIMATE=10`
  - Reason: full buffer readback per step stalls the queue with large meshes
- Shrink voxel padding without changing cutoff/usable:
  - `WAYVERB_VOXEL_PAD=3` (default is 5)
- Recommended single‑band defaults for heavy scenes:
  - cutoff 500–800 Hz, usable 0.60 (clamped), img‑src 3–4, rays ~1e5–2e5.

## Metal Backend (in progress)

Files and flags:

- Build flag: `WAYVERB_ENABLE_METAL=ON` (CMake)
- Runtime selector: `WAYVERB_METAL=1` (use Metal), `WAYVERB_METAL=force-opencl` (fallback)
- Module: `src/metal/*` (Objective‑C++), linked as `wayverb_metal` when enabled
- Factory hook: `src/combined/src/waveguide_base.cpp` selects Metal backend when enabled at runtime; otherwise OpenCL canonical path
- Current status: scaffold only; logs “[metal] backend scaffold…” and falls back to OpenCL. Next commits will replace waveguide kernels with Metal compute.

Planned implementation (Phase A):

- Compute Graph for the per‑step pipeline (pre → pressure → boundary1/2/3 → post)
- Argument Buffers to minimize binding overhead
- MTLHeaps (Private) and buffer aliasing for current/previous/history (no copies)
- Multi‑buffering for overlap; counter sampling + os_signpost for profiling

Expected wins (steady state): lower CPU submit, less memory traffic, better GPU occupancy; 10–30% wall‑time improvement before kernel‑level tuning.

## Environment Variables (summary)

- `CL_LOG_ERRORS=stdout` — print OpenCL build/runtime errors to stdout
- `WAYVERB_LOG_DIR=<dir>` — crash logs destination
- `WAYVERB_DISABLE_VIZ=1` — disable waveguide visualization readbacks
- `WAYVERB_VIZ_DECIMATE=<N>` — readback every N steps
- `WAYVERB_VOXEL_PAD=<int>` — voxel padding (default 5)
- `WAYVERB_ENABLE_METAL=ON` (build) + `WAYVERB_METAL=1` (runtime) — use Metal backend (when implemented); `force-opencl` forces fallback
- Debug/tracing (optional): `WAYVERB_WG_TRACE=1`, `WAYVERB_TRACE_NODE=<idx>`, `WAYVERB_MAX_STEPS=<N>`

## Known Issues & Tips

- Using the OBJ at original scale (~122×161×117 m) creates a huge domain. Ensure the in‑app scene is the scaled one (~36.6×48.3×35.1 m) or reduce padding.
- `usable portion` is clamped to `[0.10, 0.60]` in the model (UI allows 0.60 max) to avoid unstable/very expensive configs.

## External research via ChatGPT Pro 5

If deeper background or papers are required (Metal Compute Graphs, MPSRayIntersector details, waveguide numerics), agents are allowed to request the maintainer to trigger external ChatGPT Pro 5. In chat, address the creator as “@creator”, specify what information is missing and why it’s needed. The maintainer will provide summaries/links to proceed.

## Handover Checklist

1) Build tools with CMake; launch app via `tools/run_wayverb.sh` with `WAYVERB_LOG_DIR` set.
2) For heavy runs: `WAYVERB_DISABLE_VIZ=1` and `WAYVERB_VOXEL_PAD=3`.
3) To test Metal path (scaffold): build with `-DWAYVERB_ENABLE_METAL=ON` and run with `WAYVERB_METAL=1` — expect fallback log until kernels land.
4) Use `scripts/monitor_app_log.sh` to watch key events.
5) Record mesh summary (inside nodes, spacing) from the app log or the regression executable.
