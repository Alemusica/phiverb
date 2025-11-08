# Agent Operating Instructions (Wayverb)

Scope: entire repository

This file gives guidance to agentic assistants (Codex CLI or similar) working in this repo so they can operate safely, efficiently, and in alignment with the project’s current direction.

Core goals
- Maintain stability of the UI application and headless tools.
- Prioritize Apple Silicon performance work (Metal backend) while preserving the OpenCL path as a stable fallback.
- Keep logs and diagnostics rich enough for offline triage.

Key build flags and env
- CMake build flag to enable Metal scaffold: `-DWAYVERB_ENABLE_METAL=ON`
- Runtime selector for Metal: `WAYVERB_METAL=1` (use Metal), `WAYVERB_METAL=force-opencl` (force OpenCL fallback)
- Performance toggles (OpenCL and future Metal):
  - `WAYVERB_DISABLE_VIZ=1` disable GPU→CPU readbacks of waveguide node pressures
  - `WAYVERB_VIZ_DECIMATE=N` update pressures every N steps
  - `WAYVERB_VOXEL_PAD=<int>` shrink/expand voxel padding (default 5)

Logging
- Launch the app using `tools/run_wayverb.sh` so logs go to `build/logs/app/wayverb-*.log` and crash logs to `~/Library/Logs/Wayverb` (or `WAYVERB_LOG_DIR`).
- Use `scripts/monitor_app_log.sh` to watch filtered diagnostics.
- Check `geometrie_wayverb/` into git with every mesh/preset we debug, and reference those filenames (plus the CLI params or preset name) when collecting logs; headless runs can then use `--scene geometri_wayverb/<name>.obj` to reproduce issues.

Build provenance
- Every UI build must make its identity obvious in the title bar. The window name now includes the version, git describe, branch and timestamp derived from `core/build_id.h` / `wayverb/Source/Application.cpp`. Do not hand off binaries unless the title shows the expected commit.
- When packaging or debugging, capture the build label (screenshot or log line from the app launch) together with the relevant git commit so we can map regressions.
- CMake automatically injects `WAYVERB_BUILD_*` macros (see root `CMakeLists.txt`). If you trigger Xcode/JUCE builds directly, propagate the same macros or export an equivalent via the environment so the UI label stays accurate.
- Before investigating render regressions, regenerate `wayverb.app` via Xcode so the title bar reflects the new commit/timestamp; the pre-build script now writes `wayverb/Source/build_id_override.h` automatically.

Apple Silicon Metal port (in progress)
- Work happens on `feature/metal-apple-silicon`.
- See `docs/metal_port_plan.md` and `docs/APPLE_SILICON.md` for plan, status, and how to build.
- Do not remove or break the OpenCL backend; all Metal work must fall back cleanly.

External research policy (per maintainer)
- If additional background/papers/specs are needed (e.g. Metal Compute Graphs, MPSRayIntersector, waveguide numerics), the agent may request the maintainer to fetch or summarize via external ChatGPT Pro 5.
- Mechanism: write a short note in chat addressed to the creator (“@creator”) describing the missing info and the exact questions/sources requested. Wait for their reply before proceeding when the information is blocker‑critical.

Coding style
- Keep changes minimal and localized. Avoid API breakage.
- Update docs whenever behavior or flags change.
- Prefer adding feature flags over altering defaults for existing users.

Validation
- When adding performance features, provide a loggable path to compare timings (e.g., event profiling or printed per-stage durations) but keep it opt-in via env flags.
- Before claiming a fix for “All channels are silent”, run at least one UI render launched via `tools/run_wayverb.sh > build/logs/app/wayverb-<stamp>.log` and attach the build label plus the filtered log (`scripts/monitor_app_log.sh`). This keeps every agent focused on the same evidence instead of parallel guesswork.
- Use `bin/apple_silicon_regression` for fast headless smoke tests. It now supports CLI overrides (`--scene`, `--source`, `--receiver`, etc.) and fails if the generated channel contains NaN/Inf or is silent, so always run it on new scenes committed under `geometri_wayverb/` before opening the UI.
