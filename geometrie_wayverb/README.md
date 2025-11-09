# geometrie_wayverb

Sanitized meshes used for regression/QA pipelines. Keep every OBJ that
reproduces a bug or validation target under this directory so that headless
runs can load them via `--scene geometrie_wayverb/<name>.obj`.

Workflow:
- Sanitize with `scripts/sanitize_mesh.py` if needed, respecting metre scale.
- Precompute SDF/DIF using `scripts/waveguide_precompute.py` (emits
  `<name>.sdf.json`, `<name>.{sdf,normals,labels}.bin` e `<name>.dif.json`)
  so the runtime can skip the OpenCL “closest triangle” kernels.
- Reference the mesh plus generated `.sdf.npz`/`.dif.json` files in logs.
- Current fixtures: `shoebox_small` (6x4x3 m) e `shoebox_long` (10x6x4 m),
  entrambi con materiale `floor/ceiling/walls` definito in `shoebox_small.mtl`.
