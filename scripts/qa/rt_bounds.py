import numpy as np

ABS_BUFFER = 0.02  # ±2% guard band around the theoretical RT window

def sabine(volume, surface, alpha_bar):
    A = surface * alpha_bar
    return 0.161 * volume / max(A, 1e-9)

def eyring(volume, surface, alpha_bar):
    alpha_bar = np.clip(alpha_bar, 1e-9, 1 - 1e-9)
    return 0.161 * volume / (-surface * np.log(1.0 - alpha_bar))

def norris_eyring(volume, surface, alpha_bar, m_air):
    alpha_bar = np.clip(alpha_bar, 1e-9, 1 - 1e-9)
    denom = (-surface * np.log(1.0 - alpha_bar) + 4.0 * m_air * volume)
    return 0.161 * volume / max(denom, 1e-9)

def within_bounds(T, volume, surface, alpha_bar, m_air=0.0, tol=0.15):
    Ts = sabine(volume, surface, alpha_bar)
    Te = eyring(volume, surface, alpha_bar)
    Tn = norris_eyring(volume, surface, alpha_bar, m_air) if m_air > 0 else Te

    lo_raw, hi_raw = (min(Ts, Te), max(Ts, Te))
    lo_guard = lo_raw * (1.0 - ABS_BUFFER)
    hi_guard = hi_raw * (1.0 + ABS_BUFFER)
    cond_bounds = lo_guard <= T <= hi_guard

    best = min([Ts, Tn], key=lambda ref: abs(ref - T))
    cond_tol = abs(T - best) / best <= tol

    models = {
        "sabine": Ts,
        "eyring": Te,
        "norris": Tn,
        "best": best,
        "raw_bounds": (lo_raw, hi_raw),
        "guard_bounds": (lo_guard, hi_guard),
        "buffer": ABS_BUFFER,
    }
    return cond_bounds, cond_tol, models
