Title: <area>: <short description>

Relates: AP-<CLUSTER>-00X (e.g., AP-RT-001 or AP-DWM-002)

Summary
- What changed (1–3 lines):
- Why (link spec: docs_source/boundary.md / docs/metal_port_plan.md):
- Validation (commands + logs):

Changed files (key)
- src/...:
- docs/action_plan.md: updated checklist [yes/no]

Checklist — RT (use only if this PR touches src/raytracer/)
- [ ] BRDF helpers compile on OpenCL C; kernels build without warnings
- [ ] Deterministic diffuse rain implemented; throughput uses f·cos/pdf
- [ ] Shoebox QA passes (±1 sample / ±0.5 dB); attach results
- [ ] CI rt-tests green; metal-smoke run if shared code changed

Checklist — DWM (use only if this PR touches src/waveguide/)
- [ ] Boundary SoA (Morton) + single boundary kernel; AoS removed
- [ ] PCS transparent builder integrated; clamp/guard; multi-band safe
- [ ] Guard‑tag verified in kernels; NaN/Inf fail‑fast with SDF/DIF diagnostics
- [ ] Regressions run (WAYVERB_ALLOW_SILENT_FALLBACK=0); attach [waveguide] logs
- [ ] CI dwm-tests green; metal-smoke run if shared code changed

Notes
- Use Token‑Guard before long outputs; checkpoint at 20/60/80/90%.
