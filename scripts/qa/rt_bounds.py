import numpy as np

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
    lo, hi = (min(Ts, Te), max(Ts, Te))
    cond_bounds = lo <= T <= hi
    best = min([Ts, Tn], key=lambda ref: abs(ref - T))
    cond_tol = abs(T - best) / best <= tol
    return cond_bounds, cond_tol, (Ts, Te, Tn, best)
