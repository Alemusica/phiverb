# Mesh Tools (wayverb-mesh)

Utilities for scaling and sanitizing OBJ scenes before importing them in Wayverb.

## Dependencies

Install into a virtual environment, Apple Silicon wheels preferred:

```
pip install numpy>=1.26 trimesh>=4.0 typer>=0.12 rich>=13
# optional
pip install open3d>=0.19 pymeshlab>=2023.12
```

## CLI Usage

```
python3 tools/wayverb_mesh.py scale   in.obj out.obj --factor 0.25 --report scale.json
python3 tools/wayverb_mesh.py sanitize geometri_wayverb/scene.obj sanitized.obj \
    --factor 1.0 --report sanitize.json
```

- `scale` multiplies only `v` lines (positions), preserving `vn`, `vt`, `mtllib`, `usemtl` entries.
- `sanitize` performs triangulation, duplicate/degenerate removal, optional hole filling, normal fixes, and emits a header comment plus a JSON report if requested. `geometri_wayverb/` retains the canonical sanitized assets checked into git.

## Swift Helper

`tools/swift/ScaleOBJ.swift` exposes `scaleOBJ(inputURL:outputURL:scale:)` using Model I/O for lightweight scaling on macOS/Apple Silicon (e.g., integrate into an Xcode tool target).

## Future Work

- Mesh preflight estimator (memory/runtime prediction based on sanitized mesh + host hardware) to warn before starting expensive renders.
