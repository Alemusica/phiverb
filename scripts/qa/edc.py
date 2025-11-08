import numpy as np

def edc(ir: np.ndarray) -> np.ndarray:
    e = ir.astype(np.float64) ** 2
    return np.cumsum(e[::-1])[::-1]

def txx(ir: np.ndarray, fs: float, lo_db: float, hi_db: float):
    E = edc(ir)
    E /= (E[0] if E[0] > 0 else 1.0)
    db = 10 * np.log10(np.maximum(E, 1e-300))
    t = np.arange(len(db)) / fs

    def idx_at(level_db: float) -> int:
        idx = np.argmax(db <= level_db)
        return int(np.clip(idx, 1, len(db) - 1))

    i0 = idx_at(lo_db)
    i1 = idx_at(hi_db)
    if i1 <= i0 + 5:
        raise AssertionError(
                f"EDC troppo corta per fit tra {lo_db} e {hi_db} dB (idx {i0}->{i1}).")
    x = t[i0:i1]
    y = db[i0:i1]
    A = np.vstack([x, np.ones_like(x)]).T
    m, q = np.linalg.lstsq(A, y, rcond=None)[0]
    T = -60.0 / m
    return T, (m, q), (t, db)

def t20_t30(ir: np.ndarray, fs: float):
    T20, _, _ = txx(ir, fs, -5.0, -25.0)
    T30, _, _ = txx(ir, fs, -5.0, -35.0)
    return T20, T30
