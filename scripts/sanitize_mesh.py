#!/usr/bin/env python3

"""
Mesh pre-sanitizer using trimesh.

Pipeline (configurable via flags):
 - Load mesh (OBJ/STL/PLY/etc.)
 - Remove unreferenced verts / duplicate + degenerate faces
 - Weld vertices within epsilon
 - Optional: fill small holes, keep largest component
 - Optional: voxel remesh (band-limit) at a given pitch
 - Recompute normals, fix winding
 - Export sanitized mesh and print a JSON report to stdout

Requires: pip install trimesh shapely networkx
"""

import argparse
import json
import os
import sys

def _import_trimesh():
    try:
        import trimesh
        return trimesh
    except Exception as e:
        print(json.dumps({
            "ok": False,
            "error": f"trimesh not available: {e}",
            "hint": "pip install trimesh shapely networkx"
        }), file=sys.stderr)
        sys.exit(2)


def analyze(mesh):
    import numpy as np
    ang = None
    try:
        tri = mesh.triangles
        # compute triangle angles
        a = (tri[:,1] - tri[:,0])
        b = (tri[:,2] - tri[:,1])
        c = (tri[:,0] - tri[:,2])
        def angle(u, v):
            nu = np.linalg.norm(u, axis=1)
            nv = np.linalg.norm(v, axis=1)
            cos = (u*v).sum(axis=1) / np.maximum(nu*nv, 1e-20)
            cos = np.clip(cos, -1.0, 1.0)
            return np.degrees(np.arccos(cos))
        ang = np.vstack([angle(-c, a), angle(-a, b), angle(-b, c)])
        min_angle = float(np.nanmin(ang)) if ang.size else None
        max_angle = float(np.nanmax(ang)) if ang.size else None
    except Exception:
        min_angle = max_angle = None

    return {
        "vertices": int(mesh.vertices.shape[0]),
        "faces": int(mesh.faces.shape[0]),
        "watertight": bool(getattr(mesh, 'is_watertight', False)),
        "euler_number": getattr(mesh, 'euler_number', None),
        "min_angle_deg": min_angle,
        "max_angle_deg": max_angle,
        "degenerate_faces": int(getattr(mesh, 'faces_sparse', None).shape[0] if getattr(mesh, 'faces_sparse', None) is not None else 0)
    }


def main():
    tm = _import_trimesh()
    ap = argparse.ArgumentParser()
    ap.add_argument('input', help='input mesh (OBJ/STL/PLY/…)')
    ap.add_argument('-o', '--output', help='output path (default: <input>.san.obj)')
    ap.add_argument('--weld-eps', type=float, default=1e-6, help='vertex weld epsilon (units of mesh)')
    ap.add_argument('--fill-holes', action='store_true', help='fill small holes')
    ap.add_argument('--keep-largest', action='store_true', help='keep only largest connected component')
    ap.add_argument('--voxel-pitch', type=float, default=None, help='optional voxel remesh pitch (band-limit geometry)')
    args = ap.parse_args()

    inp = args.input
    outp = args.output or (os.path.splitext(inp)[0] + '.san.obj')

    scene_or_mesh = tm.load(inp, process=False)
    if isinstance(scene_or_mesh, tm.Scene):
        meshes = list(scene_or_mesh.dump())
        mesh = tm.util.concatenate(meshes) if meshes else None
    else:
        mesh = scene_or_mesh
    if mesh is None:
        print(json.dumps({"ok": False, "error": "failed to load mesh"}))
        sys.exit(1)

    # Basic cleanup
    mesh.remove_unreferenced_vertices()
    mesh.remove_duplicate_faces()
    mesh.remove_degenerate_faces()
    try:
        mesh.merge_vertices(eps=args.weld_eps)
    except Exception:
        pass

    # Fix normals & winding
    try:
        tm.repair.fix_winding(mesh)
        tm.repair.fix_normals(mesh)
    except Exception:
        pass

    if args.fill_holes:
        try:
            mesh.fill_holes()
        except Exception:
            pass

    if args.keep_largest:
        comp = mesh.split(only_watertight=False)
        if comp:
            comp.sort(key=lambda m: m.area if hasattr(m, 'area') else len(m.faces), reverse=True)
            mesh = comp[0]

    if args.voxel_pitch and args.voxel_pitch > 0:
        try:
            vox = mesh.voxelized(args.voxel_pitch)
            # marching_cubes gives a surface approximation rather than boxes
            mc = vox.marching_cubes
            if mc is not None:
                mesh = mc
        except Exception:
            pass

    # Final pass
    mesh.remove_unreferenced_vertices()
    mesh.remove_duplicate_faces()
    mesh.remove_degenerate_faces()

    rep = analyze(mesh)
    mesh.export(outp)
    print(json.dumps({"ok": True, "output": outp, "report": rep}))

if __name__ == '__main__':
    main()

