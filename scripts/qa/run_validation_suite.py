import argparse
import json
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

from edc import t20_t30
from rt_bounds import within_bounds

def run_cli(cli, scene_json, wav_out):
    cmd = [cli, "--scene", str(scene_json), "--out", str(wav_out)]
    subprocess.run(cmd, check=True)

def validate_ir(wav, fs_expect, volume, surface, alpha_bar, m_air, tol):
    ir, fs = sf.read(str(wav), dtype="float64")
    if fs_expect and abs(fs - fs_expect) > 1:
        raise AssertionError(f"Sample rate inatteso: {fs} != {fs_expect}")
    if ir.ndim > 1:
        ir = ir[:, 0]
    T20, T30 = t20_t30(ir, fs)
    T = T30 if np.isfinite(T30) else T20
    in_bounds, in_tol, models = within_bounds(
            T, volume, surface, alpha_bar, m_air, tol)
    if not in_bounds:
        raise AssertionError(
                f"T={T:.3f}s fuori dal range Sabine/Eyring {models[0]:.3f}-{models[1]:.3f}")
    if not in_tol:
        raise AssertionError(
                f"T={T:.3f}s oltre ±{int(tol*100)}% dal modello best={models[3]:.3f}s")
    return T20, T30, models

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cli", required=True)
    ap.add_argument("--scenes", default="tests/scenes")
    ap.add_argument("--fs", type=int, default=48000)
    ap.add_argument("--tol", type=float, default=0.15)
    args = ap.parse_args()

    ok = True
    scenes_path = Path(args.scenes)
    for scene_json in sorted(scenes_path.glob("*.json")):
        meta = json.loads(scene_json.read_text())
        wav_out = scene_json.with_suffix(".wav")
        run_cli(args.cli, scene_json, wav_out)
        try:
            T20, T30, models = validate_ir(
                    wav_out,
                    args.fs,
                    meta["volume_m3"],
                    meta["surface_m2"],
                    meta["alpha_bar_1k"],
                    meta.get("air_m_nepers_per_m", 0.0),
                    args.tol)
            print(f"[QA] {scene_json.name}: T20={T20:.3f}s T30={T30:.3f}s models={tuple(round(m,3) for m in models)}")
        except Exception as exc:
            ok = False
            print(f"[QA][FAIL] {scene_json.name}: {exc}")
    sys.exit(0 if ok else 1)

if __name__ == "__main__":
    main()
