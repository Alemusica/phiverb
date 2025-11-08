#!/usr/bin/env python3
"""
wayverb-mesh: utilities for scaling and sanitizing OBJ meshes used by Wayverb.

Two primary commands are exposed via Typer:

    python3 tools/wayverb_mesh.py scale in.obj out.obj --factor 0.25
    python3 tools/wayverb_mesh.py sanitize in.obj out.obj --report report.json

Dependencies (installable via pip, preferably in a venv):
    numpy>=1.26
    trimesh>=4.0
    open3d>=0.19 (optional, improves non-manifold cleanup)
    pymeshlab>=2023.12 (optional, advanced repairs)
    typer>=0.12
    rich>=13
"""

from __future__ import annotations

import datetime as _dt
import json
import math
import tempfile
from pathlib import Path
from typing import Iterable, List, Optional

import typer
from rich.console import Console
from rich.progress import Progress

console = Console()
app = typer.Typer(help="Wayverb mesh utilities (scale / sanitize).")


def _read_lines(path: Path) -> List[str]:
    return path.read_text(encoding="utf-8").splitlines()


def _write_lines(path: Path, lines: Iterable[str]) -> None:
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _scale_vertex_line(line: str, factor: float) -> str:
    parts = line.strip().split()
    if len(parts) < 4:
        return line
    scaled = [
        f"{float(parts[1]) * factor:.9f}",
        f"{float(parts[2]) * factor:.9f}",
        f"{float(parts[3]) * factor:.9f}",
    ]
    return "v " + " ".join(scaled)


@app.command()
def scale(
        input_path: Path = typer.Argument(..., exists=True, dir_okay=False),
        output_path: Path = typer.Argument(..., dir_okay=False),
        factor: float = typer.Option(
                1.0, "--factor", "-f", help="Uniform scale factor for vertex positions."),
        report: Optional[Path] = typer.Option(
                None, "--report", help="Optional JSON report (bbox before/after)."),
) -> None:
    """Scale only the vertex (`v`) positions of an OBJ file, preserving all other records."""
    if factor == 0:
        raise typer.BadParameter("Scale factor cannot be zero.")

    lines = _read_lines(input_path)
    scaled_lines: List[str] = []

    bbox_min = [math.inf, math.inf, math.inf]
    bbox_max = [-math.inf, -math.inf, -math.inf]

    for line in lines:
        if line.startswith("v ") and not line.startswith("vn"):
            parts = line.split()
            coords = list(map(float, parts[1:4]))
            for i in range(3):
                bbox_min[i] = min(bbox_min[i], coords[i])
                bbox_max[i] = max(bbox_max[i], coords[i])
            scaled_lines.append(_scale_vertex_line(line, factor))
        else:
            scaled_lines.append(line)

    _write_lines(output_path, scaled_lines)
    console.print(f"[green]Scaled OBJ written to {output_path}[/green]")

    if report:
        before_size = [bbox_max[i] - bbox_min[i] for i in range(3)]
        scaled_size = [dim * factor for dim in before_size]
        payload = {
            "input": str(input_path),
            "output": str(output_path),
            "factor": factor,
            "bbox_before": {"min": bbox_min, "max": bbox_max, "size": before_size},
            "bbox_after": {"size": scaled_size},
            "timestamp": _dt.datetime.now().isoformat(),
        }
        report.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        console.print(f"[blue]Report written to {report}[/blue]")


def _lazy_import_trimesh():
    try:
        import trimesh  # type: ignore
    except ImportError as exc:
        raise typer.BadParameter(
                "trimesh is required for sanitize. Install via `pip install trimesh`.") from exc
    return trimesh


def _lazy_import_numpy():
    try:
        import numpy as np  # type: ignore
    except ImportError as exc:
        raise typer.BadParameter(
                "numpy is required for sanitize. Install via `pip install numpy`.") from exc
    return np


def _try_open3d_cleanup(mesh) -> None:
    try:
        import open3d as o3d  # type: ignore
    except ImportError:
        console.print("[yellow]open3d not installed; skipping non-manifold cleanup.[/yellow]")
        return
    o3d_mesh = o3d.geometry.TriangleMesh(
            vertices=o3d.utility.Vector3dVector(mesh.vertices),
            triangles=o3d.utility.Vector3iVector(mesh.faces))
    o3d_mesh.remove_duplicated_vertices()
    o3d_mesh.remove_degenerate_triangles()
    o3d_mesh.remove_unreferenced_vertices()
    o3d_mesh.remove_non_manifold_edges()
    mesh.vertices = _lazy_import_numpy().asarray(o3d_mesh.vertices)
    mesh.faces = _lazy_import_numpy().asarray(o3d_mesh.triangles)


def _collect_metadata(lines: List[str]) -> dict:
    mtllib = [ln for ln in lines if ln.lower().startswith("mtllib ")]
    usemtl = [ln for ln in lines if ln.lower().startswith("usemtl ")]
    header = [
        f"# sanitized by wayverb-mesh on {_dt.datetime.now().isoformat()}",
        f"# mtllib preserved: {len(mtllib)} entry(ies)",
        f"# original usemtl count: {len(usemtl)}",
    ]
    return {"header": header, "mtllib": mtllib, "usemtl": usemtl}


@app.command()
def sanitize(
        input_path: Path = typer.Argument(..., exists=True, dir_okay=False),
        output_path: Path = typer.Argument(..., dir_okay=False),
        factor: float = typer.Option(1.0, "--factor", help="Optional uniform scale before cleanup."),
        triangulate: bool = typer.Option(True, "--triangulate/--no-triangulate"),
        fill_holes: bool = typer.Option(True, "--fill-holes/--no-fill-holes"),
        fix_normals: bool = typer.Option(True, "--fix-normals/--no-fix-normals"),
        recompute_normals: bool = typer.Option(
                False, "--recompute-normals",
                help="Force recomputation of normals after cleanup."),
        report: Optional[Path] = typer.Option(None, "--report", help="Optional JSON report path."),
) -> None:
    """
    Sanitize an OBJ file:
      * optional scaling
      * triangulation (fan)
      * duplicate/degenerate/non-manifold cleanup
      * fills small holes (optionally)
      * writes metadata header + preserves mtllib/usemtl declarations when possible
    """
    trimesh = _lazy_import_trimesh()
    np = _lazy_import_numpy()

    original_lines = _read_lines(input_path)
    metadata = _collect_metadata(original_lines)

    mesh = trimesh.load(input_path, force="mesh", process=False)
    if factor != 1.0:
        mesh.apply_scale(factor)

    if triangulate:
        mesh = mesh.triangulate()

    with Progress() as progress:
        task = progress.add_task("[cyan]Repairing mesh...", total=6)
        mesh.remove_duplicate_faces()
        progress.advance(task)
        mesh.remove_degenerate_faces()
        progress.advance(task)
        mesh.remove_duplicate_vertices()
        progress.advance(task)
        mesh.remove_infinite_values()
        progress.advance(task)
        mesh.remove_unreferenced_vertices()
        progress.advance(task)
        if fix_normals:
            mesh.fix_normals()
        if fill_holes and mesh.is_watertight is False:
            try:
                mesh.fill_holes()
            except Exception as exc:  # pragma: no cover - best effort
                console.print(f"[yellow]fill_holes skipped: {exc}[/yellow]")
        _try_open3d_cleanup(mesh)
        progress.update(task, advance=1, completed=6)

    if recompute_normals:
        mesh.rezero()
        mesh.fix_normals()

    bounds = mesh.bounds
    bbox = {"min": bounds[0].tolist(), "max": bounds[1].tolist()}

    with tempfile.TemporaryDirectory() as tmpdir:
        temp_path = Path(tmpdir) / "tmp.obj"
        mesh.export(temp_path)
        processed_lines = [
            ln for ln in temp_path.read_text(encoding="utf-8").splitlines()
            if not ln.lower().startswith("mtllib ")
        ]

    output_lines: List[str] = []
    output_lines.extend(metadata["header"])
    output_lines.extend(metadata["mtllib"])
    default_usemtl = metadata["usemtl"][0] if len(set(metadata["usemtl"])) == 1 else None
    if default_usemtl:
        output_lines.append(default_usemtl)
    output_lines.extend(processed_lines)

    _write_lines(output_path, output_lines)
    console.print(f"[green]Sanitized OBJ written to {output_path}[/green]")

    if report:
        payload = {
            "input": str(input_path),
            "output": str(output_path),
            "scale_factor": factor,
            "triangulate": triangulate,
            "fill_holes": fill_holes,
            "fix_normals": fix_normals,
            "recompute_normals": recompute_normals,
            "vertex_count": int(mesh.vertices.shape[0]),
            "face_count": int(mesh.faces.shape[0]),
            "bbox": bbox,
            "timestamp": _dt.datetime.now().isoformat(),
        }
        report.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        console.print(f"[blue]Report written to {report}[/blue]")


if __name__ == "__main__":
    app()
