#!/usr/bin/env python3
"""
Convert a SOFA HRTF dataset into the WAV layout expected by Wayverb.

Each output file follows the naming pattern:
    {tag}_R000_T{azimuth}_P{elevation}.wav
Where azimuth/elevation are rounded to integer degrees (0-359).
"""

from __future__ import annotations

import argparse
from pathlib import Path

import h5py
import numpy as np
import soundfile as sf


def _decode_attr(value: object) -> str:
    if isinstance(value, (bytes, bytearray)):
        return value.decode("utf-8", errors="ignore")
    if isinstance(value, np.ndarray):
        # HDF5 string attributes may come back as char arrays
        return "".join(chr(c) for c in value.tolist())
    return str(value)


def _polar_to_degrees(source_positions: np.ndarray, units: str) -> tuple[np.ndarray, np.ndarray]:
    az = source_positions[:, 0].astype(float)
    el = source_positions[:, 1].astype(float)

    if "rad" in units.lower():
        az = np.degrees(az)
        el = np.degrees(el)

    az = np.mod(np.round(az), 360).astype(int)
    el = np.mod(np.round(el), 360).astype(int)

    return az, el


def _cartesian_to_degrees(source_positions: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    x, y, z = source_positions[:, 0], source_positions[:, 1], source_positions[:, 2]
    r = np.sqrt(x * x + y * y + z * z) + 1e-12
    az = np.degrees(np.arctan2(y, x)) % 360
    el = np.degrees(np.arcsin(z / r)) % 360
    return az.astype(int), el.astype(int)


def _snap(values: np.ndarray, step: int) -> np.ndarray:
    if step <= 0:
        return values
    snapped = np.round(values / step) * step
    return snapped


def convert_sofa_to_wav(
    sofa_path: Path,
    out_dir: Path,
    tag: str,
    snap_az: int = 1,
    snap_el: int = 1,
    mirror_elevation: bool = False,
) -> None:
    with h5py.File(sofa_path, "r") as sofa:
        ir = sofa["Data.IR"][...]  # (M, R=2, N)
        if ir.ndim != 3 or ir.shape[1] != 2:
            raise ValueError("Dataset must contain stereo IRs (Data.IR shape Mx2xN).")

        sr_ds = sofa["Data.SamplingRate"][...]
        sample_rate = float(sr_ds if np.isscalar(sr_ds) else sr_ds[0])

        source_positions = sofa["SourcePosition"][...]
        position_type = _decode_attr(sofa["SourcePosition"].attrs.get("Type", ""))
        position_units = _decode_attr(sofa["SourcePosition"].attrs.get("Units", ""))

        if "cartesian" in position_type.lower():
            azimuth, elevation = _cartesian_to_degrees(source_positions)
        else:
            azimuth, elevation = _polar_to_degrees(source_positions, position_units)

    azimuth = _snap(azimuth, snap_az)
    elevation = _snap(elevation, snap_el)

    azimuth = np.mod(np.round(azimuth), 360).astype(int)
    if mirror_elevation:
        elevation = np.abs(elevation)
        over = elevation > 180
        elevation[over] = 360 - elevation[over]
    elevation = np.mod(np.round(elevation), 360).astype(int)

    out_dir.mkdir(parents=True, exist_ok=True)
    for idx, (az, el) in enumerate(zip(azimuth, elevation)):
        data = ir[idx].T  # -> samples x channels
        filename = out_dir / f"{tag}_R000_T{az:03d}_P{el:03d}.wav"
        sf.write(filename, data, int(round(sample_rate)))

    print(f"Written {len(azimuth)} impulse responses to {out_dir}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sofa", type=Path, help="Path to the input .sofa file")
    parser.add_argument(
        "out_dir",
        type=Path,
        help="Destination directory (e.g. src/core/data/IRC_CUSTOM)",
    )
    parser.add_argument(
        "--tag",
        default="IRC_CUSTOM",
        help="Filename prefix (default: IRC_CUSTOM)",
    )
    parser.add_argument(
        "--snap-az",
        type=int,
        default=1,
        help="Snap azimuth degrees to this step (default: 1, i.e., no snapping)",
    )
    parser.add_argument(
        "--snap-el",
        type=int,
        default=1,
        help="Snap elevation degrees to this step (default: 1)",
    )
    parser.add_argument(
        "--mirror-el",
        action="store_true",
        help="Mirror elevations across the horizontal plane (useful for datasets given in ± angles).",
    )
    args = parser.parse_args()

    convert_sofa_to_wav(
        args.sofa,
        args.out_dir,
        args.tag,
        snap_az=args.snap_az,
        snap_el=args.snap_el,
        mirror_elevation=args.mirror_el,
    )


if __name__ == "__main__":
    main()
