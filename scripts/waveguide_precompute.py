#!/usr/bin/env python3
"""
Precompute Signed Distance Fields (SDF), normals, and Digital Impedance
Filters (DIF) for waveguide scenes. The generated artifacts live next to the
source mesh and are consumed directly by the runtime (see
`waveguide/precomputed_inputs.*`).

Example usage:

    scripts/waveguide_precompute.py \
        --mesh geometrie_wayverb/shoebox_small.obj \
        --voxel-pitch 0.1 \
        --voxel-pad 4 \
        --sdf-out geometrie_wayverb/shoebox_small.sdf.npz \
        --dif-config assets/materials/ptb_shoebox.json \
        --dif-out geometrie_wayverb/shoebox_small.dif.json
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Tuple

import numpy as np
import trimesh


# -----------------------------------------------------------------------------
# Mesh utilities


def _parse_obj_materials(mesh_path: Path, face_count: int) -> Tuple[List[str], np.ndarray]:
    """
    Parse the OBJ file to extract `usemtl` assignments per face. Returns
    (ordered_material_names, face_label_ids) where label_ids aligns with the
    face order used by trimesh (process=False keeps the original ordering).
    """
    materials: List[str] = []
    face_labels: List[int] = []
    current = "default"
    label_to_id: Dict[str, int] = {"default": 0}
    materials.append("default")

    with mesh_path.open("r", encoding="utf-8") as fh:
        for raw in fh:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("usemtl"):
                tokens = line.split()
                if len(tokens) >= 2:
                    current = tokens[1]
                    if current not in label_to_id:
                        label_to_id[current] = len(materials)
                        materials.append(current)
            elif line.startswith("f "):
                face_labels.append(label_to_id[current])

    if len(face_labels) != face_count:
        raise RuntimeError(
            f"Face count mismatch when parsing {mesh_path}: "
            f"OBJ faces={len(face_labels)} vs mesh faces={face_count}"
        )

    return materials, np.asarray(face_labels, dtype=np.int16)


# -----------------------------------------------------------------------------
# Distance + label computation helpers


def signed_distance_safe(mesh: trimesh.Trimesh, points: np.ndarray) -> np.ndarray:
    """
    Wrapper around trimesh.proximity.signed_distance that falls back to the
    naive implementation when libspatialindex/rtree is unavailable.
    """
    pts = np.asanyarray(points, dtype=np.float64)
    try:
        return trimesh.proximity.signed_distance(mesh, pts)
    except ModuleNotFoundError as exc:
        if getattr(exc, "name", "") != "rtree":
            raise

    closest, distance, tri_id = trimesh.proximity.closest_point_naive(mesh, pts)
    tol = trimesh.constants.tol.merge
    nonzero_idx = np.where(distance > tol)[0]
    if nonzero_idx.size == 0:
        return distance

    normals = mesh.face_normals
    vec = pts[nonzero_idx] - closest[nonzero_idx]
    tri = tri_id[nonzero_idx]
    valid = tri >= 0
    dot = np.einsum("ij,ij->i", normals[tri[valid]], vec[valid])
    sign = np.zeros_like(vec[:, 0])
    sign[valid] = np.sign(dot)
    distance[nonzero_idx] *= np.where(sign == 0, 1.0, -sign)
    return distance


def closest_triangles_safe(mesh: trimesh.Trimesh, points: np.ndarray) -> np.ndarray:
    try:
        _, _, tri_id = trimesh.proximity.closest_point(mesh, points)
        return tri_id
    except ModuleNotFoundError as exc:
        if getattr(exc, "name", "") != "rtree":
            raise

    _, _, tri_id = trimesh.proximity.closest_point_naive(mesh, points)
    return tri_id


def write_binary(path: Path, array: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(array.tobytes(order="C"))


def save_sdf_metadata(payload: Dict[str, np.ndarray],
                      label_names: List[str],
                      binary_dir: Path,
                      base_name: str) -> None:
    sdf_bin = binary_dir / f"{base_name}.sdf.bin"
    normals_bin = binary_dir / f"{base_name}.normals.bin"
    labels_bin = binary_dir / f"{base_name}.labels.bin"

    write_binary(sdf_bin, payload["sdf"])
    write_binary(normals_bin, payload["normals"])
    write_binary(labels_bin, payload["labels"])

    meta = {
        "origin": payload["origin"].tolist(),
        "dims": payload["dims"].tolist(),
        "voxel_pitch": float(payload["pitch"]),
        "labels": label_names,
        "files": {
            "sdf": sdf_bin.name,
            "normals": normals_bin.name,
            "labels": labels_bin.name,
        },
    }
    meta_path = binary_dir / f"{base_name}.sdf.json"
    meta_path.write_text(json.dumps(meta, indent=2))


def _build_grid(bounds: np.ndarray, pitch: float, pad: int) -> Tuple[np.ndarray, np.ndarray]:
    if pitch <= 0:
        raise ValueError("voxel pitch must be > 0")
    pad = max(pad, 0)
    min_corner = bounds[0] - pad * pitch
    max_corner = bounds[1] + pad * pitch
    dims = np.ceil((max_corner - min_corner) / pitch).astype(int) + 1
    dims = np.maximum(dims, 1)
    return min_corner.astype(np.float64), dims.astype(np.int32)


def _iter_unravel_indices(dims: np.ndarray, chunk_size: int):
    total = int(np.prod(dims))
    flat = np.arange(total, dtype=np.int64)
    for start in range(0, total, chunk_size):
        end = min(start + chunk_size, total)
        chunk_idx = flat[start:end]
        coords = np.stack(np.unravel_index(chunk_idx, dims, order="C"), axis=1)
        yield slice(start, end), coords


def compute_fields(mesh: trimesh.Trimesh,
                   pitch: float,
                   pad: int,
                   chunk_size: int,
                   face_label_ids: np.ndarray,
                   label_names: List[str]) -> Dict[str, np.ndarray]:
    origin, dims = _build_grid(mesh.bounds, pitch, pad)
    total = int(np.prod(dims))
    sdf = np.empty(total, dtype=np.float32)
    for chunk_slice, coords in _iter_unravel_indices(dims, chunk_size):
        points = origin + coords * pitch
        distances = signed_distance_safe(mesh, points)
        sdf[chunk_slice] = distances.astype(np.float32)

    sdf_volume = sdf.reshape(tuple(dims))
    grad = np.gradient(sdf_volume, pitch, edge_order=2)
    normals = np.stack(grad, axis=-1).astype(np.float32)
    norm_len = np.linalg.norm(normals, axis=-1, keepdims=True)
    np.divide(normals,
              np.maximum(norm_len, 1e-12),
              out=normals,
              where=norm_len > 0)

    labels = np.full(total, -1, dtype=np.int16)
    for chunk_slice, coords in _iter_unravel_indices(dims, chunk_size):
        points = origin + coords * pitch
        tri_ids = closest_triangles_safe(mesh, points)
        ids = np.where(
            (tri_ids >= 0) & (tri_ids < len(face_label_ids)),
            face_label_ids[tri_ids],
            -1,
        )
        labels[chunk_slice] = ids.astype(np.int16)

    return {
        "origin": origin.astype(np.float32),
        "dims": dims,
        "pitch": float(pitch),
        "sdf": sdf_volume,
        "normals": normals,
        "labels": labels.reshape(tuple(dims)),
        "label_names": label_names,
    }


# -----------------------------------------------------------------------------
# DIF helpers


def load_material_config(path: Path) -> Dict[str, Any]:
    payload = json.loads(path.read_text())
    if "bands_hz" not in payload or "materials" not in payload:
        raise ValueError("DIF config requires 'bands_hz' and 'materials' keys")
    payload.setdefault("sample_rate", 48000)
    return payload


def save_dif(payload: Dict[str, Any], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True))


# -----------------------------------------------------------------------------
# CLI / main


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--mesh", required=True, help="Input OBJ/STL/PLY mesh path")
    ap.add_argument("--voxel-pitch", type=float, default=0.05,
                    help="Voxel pitch in metres (default: 0.05)")
    ap.add_argument("--voxel-pad", type=int, default=5,
                    help="Padding voxels around the mesh (default: 5)")
    ap.add_argument("--chunk-size", type=int, default=250_000,
                    help="Points per batch for SDF computation (default: 250k)")
    ap.add_argument("--sdf-out", help="Optional NPZ snapshot of the SDF volume")
    ap.add_argument("--dif-config", help="Material JSON describing absorption per band")
    ap.add_argument("--dif-out", help="Output .json for DIF metadata")
    return ap.parse_args()


def main() -> None:
    args = parse_args()
    mesh_path = Path(args.mesh)
    if not mesh_path.exists():
        raise SystemExit(f"Mesh not found: {mesh_path}")

    loaded = trimesh.load(mesh_path, process=False)
    if isinstance(loaded, trimesh.Scene):
        meshes = list(loaded.dump())
        mesh = trimesh.util.concatenate(meshes) if meshes else None
    else:
        mesh = loaded
    if mesh is None or not isinstance(mesh, trimesh.Trimesh):
        raise SystemExit("Expected a single-mesh OBJ/STL")

    label_names, face_label_ids = _parse_obj_materials(mesh_path, len(mesh.faces))

    fields = compute_fields(mesh,
                            args.voxel_pitch,
                            args.voxel_pad,
                            args.chunk_size,
                            face_label_ids,
                            label_names)

    if args.sdf_out:
        Path(args.sdf_out).parent.mkdir(parents=True, exist_ok=True)
        np.savez_compressed(args.sdf_out,
                            origin=fields["origin"],
                            dims=fields["dims"],
                            voxel_pitch=np.array([fields["pitch"]], dtype=np.float32),
                            sdf=fields["sdf"],
                            normals=fields["normals"],
                            labels=fields["labels"])

    base_name = mesh_path.stem
    save_sdf_metadata(
        {
            "origin": fields["origin"],
            "dims": fields["dims"],
            "pitch": fields["pitch"],
            "sdf": fields["sdf"],
            "normals": fields["normals"],
            "labels": fields["labels"],
        },
        label_names,
        mesh_path.parent,
        base_name,
    )

    if args.dif_config:
        dif_cfg = load_material_config(Path(args.dif_config))
        dif_out = Path(args.dif_out) if args.dif_out else mesh_path.with_suffix(".dif.json")
        save_dif(dif_cfg, dif_out)
        print(f"[waveguide-precompute] wrote DIF => {dif_out} (materials={len(dif_cfg['materials'])})")

    print(f"[waveguide-precompute] wrote SDF artifacts for {mesh_path}")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:  # pragma: no cover - CLI guard
        print(f"[waveguide-precompute] error: {exc}", file=sys.stderr)
        raise
