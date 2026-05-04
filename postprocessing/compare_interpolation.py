#!/usr/bin/env python3
"""
compare_interpolation.py
────────────────────────
Leave-one-cluster-out cross-validation comparing three methods:
  • Linear     — Delaunay triangulation + linear interpolation (TIN)
  • Thin-plate — biharmonic / thin-plate spline (minimum curvature)
  • IDW        — inverse distance weighting (power = 2)

Outputs:
  cv_results.csv       — fold-level predictions for all methods
  cv_error_plot.png    — per-fold signed error bar chart
  cv_scatter_plot.png  — predicted vs actual scatter (one panel per method)

Usage:
  python compare_interpolation.py [data_clean.txt]
"""

from __future__ import annotations
import sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy.interpolate import Rbf, griddata
from scipy.spatial import cKDTree

# ── tunables ──────────────────────────────────────────────────────────────────
INPUT_FILE     = "data_clean.txt"
CLUSTER_EPS_M  = 3.0           # GPS jitter radius for stop clustering (metres)
MIN_CLUSTER_SZ = 3             # min raw samples to form one representative stop
IDW_POWER      = 2             # IDW distance exponent
KPA2MM         = 1000.0 / 9.80665  # kPa → mm of water column

METHODS = ["Linear (TIN)", "Thin-plate spline", "IDW (p=2)"]
COLORS  = {"Linear (TIN)": "#4fc3f7",
           "Thin-plate spline": "#a5d6a7",
           "IDW (p=2)": "#ffcc80"}


# ── helpers ───────────────────────────────────────────────────────────────────
def load(path: str) -> pd.DataFrame:
    df = pd.read_csv(path, header=None,
                     names=["lat", "lon", "kpa", "mm", "ts"],
                     skipinitialspace=True, engine="python")
    df = df.apply(pd.to_numeric, errors="coerce")
    return df.dropna(subset=["lat", "lon", "kpa", "mm"]).reset_index(drop=True)


def to_meters(lat, lon):
    lat = np.asarray(lat, float); lon = np.asarray(lon, float)
    lat0, lon0 = lat.mean(), lon.mean()
    x = (lon - lon0) * 111_320.0 * np.cos(np.deg2rad(lat0))
    y = (lat - lat0) * 111_132.0
    return x, y, lat0, lon0


def depth_m(kpa, mm):
    return (np.asarray(mm, float) + np.asarray(kpa, float) * KPA2MM) / 1000.0


def cluster_components(x, y, eps):
    pts = np.column_stack([x, y])
    tree = cKDTree(pts)
    visited = np.zeros(len(pts), bool)
    comps = []
    for i in range(len(pts)):
        if visited[i]:
            continue
        stack, comp = [i], []
        visited[i] = True
        while stack:
            k = stack.pop(); comp.append(k)
            for j in tree.query_ball_point(pts[k], eps):
                if not visited[j]:
                    visited[j] = True; stack.append(j)
        comps.append(comp)
    return comps


def aggregate(df: pd.DataFrame) -> pd.DataFrame:
    x, y, lat0, lon0 = to_meters(df["lat"], df["lon"])
    z = depth_m(df["kpa"], df["mm"])
    rows = []
    for comp in cluster_components(x, y, CLUSTER_EPS_M):
        xi, yi, zi = x[comp], y[comp], z[comp]
        ts = df["ts"].to_numpy(float)[comp]
        if len(comp) >= MIN_CLUSTER_SZ:
            rows.append(dict(x=np.median(xi), y=np.median(yi),
                             z=np.median(zi), n=len(comp),
                             ts=np.median(ts)))
        else:
            for i in range(len(comp)):
                rows.append(dict(x=xi[i], y=yi[i], z=zi[i], n=1, ts=ts[i]))
    return pd.DataFrame(rows).sort_values("ts").reset_index(drop=True)


# ── interpolation methods ─────────────────────────────────────────────────────
def pred_linear(tx, ty, tz, px, py) -> float:
    v = griddata((tx, ty), tz, (px, py), method="linear")
    return float(np.ravel(v)[0])


def pred_tps(tx, ty, tz, px, py) -> float:
    rbf = Rbf(tx, ty, tz, function="thin_plate", smooth=0.0)
    return float(rbf(float(px), float(py)))


def pred_idw(tx, ty, tz, px, py) -> float:
    d = np.hypot(tx - px, ty - py)
    if np.any(d == 0):
        return float(tz[d == 0][0])
    w = 1.0 / d**IDW_POWER
    return float(np.dot(w, tz) / w.sum())


PREDICTORS = {
    "Linear (TIN)":      pred_linear,
    "Thin-plate spline": pred_tps,
    "IDW (p=2)":         pred_idw,
}


# ── cross-validation ──────────────────────────────────────────────────────────
def cross_validate(agg: pd.DataFrame):
    x = agg["x"].to_numpy(float)
    y = agg["y"].to_numpy(float)
    z = agg["z"].to_numpy(float)
    n = len(agg)

    raw_preds = {m: np.full(n, np.nan) for m in METHODS}
    for i in range(n):
        mask = np.ones(n, bool); mask[i] = False
        for m in METHODS:
            try:
                raw_preds[m][i] = PREDICTORS[m](x[mask], y[mask], z[mask],
                                                 x[i], y[i])
            except Exception:
                pass

    # intersect valid folds across all methods for a fair comparison
    valid = np.ones(n, bool)
    for m in METHODS:
        valid &= np.isfinite(raw_preds[m])
    idx = np.where(valid)[0]

    if len(idx) == 0:
        raise RuntimeError("No common valid folds — check dataset size.")

    rows, results = [], []
    for m in METHODS:
        p = raw_preds[m][idx]
        a = z[idx]
        err = p - a
        mae  = float(np.mean(np.abs(err)))
        rmse = float(np.sqrt(np.mean(err**2)))
        maxe = float(np.max(np.abs(err)))
        results.append(dict(method=m, n_folds=len(idx),
                            mae=mae, rmse=rmse, max_err=maxe))
        for k, i in enumerate(idx):
            rows.append(dict(fold=int(i), method=m,
                             predicted=float(raw_preds[m][i]),
                             actual=float(z[i]),
                             error=float(raw_preds[m][i] - z[i])))

    return pd.DataFrame(results), pd.DataFrame(rows), idx


# ── plots ─────────────────────────────────────────────────────────────────────
def _dark(fig, axes):
    fig.patch.set_facecolor("#0d1117")
    for ax in (axes if hasattr(axes, "__iter__") else [axes]):
        ax.set_facecolor("#161b22")
        for sp in ax.spines.values(): sp.set_edgecolor("#30363d")
        ax.tick_params(colors="#8b949e")
        ax.xaxis.label.set_color("#c9d1d9")
        ax.yaxis.label.set_color("#c9d1d9")
        ax.title.set_color("#e6edf3")


def plot_errors(detail: pd.DataFrame, idx, out="cv_error_plot.png"):
    fig, ax = plt.subplots(figsize=(12, 4.5))
    bw = 0.26
    xpos = np.arange(len(idx))
    for k, m in enumerate(METHODS):
        sub = detail[detail["method"] == m].sort_values("fold")
        offset = (k - 1) * bw
        ax.bar(xpos + offset, sub["error"].to_numpy(), bw,
               label=m, color=COLORS[m], edgecolor="none")
    ax.axhline(0, color="#8b949e", linewidth=0.8, linestyle="--")
    ax.set_xticks(xpos)
    ax.set_xticklabels([str(i) for i in idx], fontsize=7)
    ax.set_xlabel("Fold (cluster index)")
    ax.set_ylabel("Error  (predicted − actual)  m")
    ax.set_title("Leave-one-cluster-out CV — per-fold signed errors")
    ax.legend(facecolor="#21262d", labelcolor="#c9d1d9", framealpha=0.8)
    _dark(fig, ax)
    plt.tight_layout()
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  Saved {out}")


def plot_scatter(detail: pd.DataFrame, out="cv_scatter_plot.png"):
    fig, axes = plt.subplots(1, 3, figsize=(14, 4.8))
    for ax, m in zip(axes, METHODS):
        sub = detail[detail["method"] == m]
        a = sub["actual"].to_numpy(float)
        p = sub["predicted"].to_numpy(float)
        mae  = np.mean(np.abs(p - a))
        rmse = np.sqrt(np.mean((p - a)**2))
        lo = min(a.min(), p.min()) * 0.95
        hi = max(a.max(), p.max()) * 1.05
        ax.scatter(a, p, color=COLORS[m], s=55, alpha=0.9, edgecolors="none", zorder=3)
        ax.plot([lo, hi], [lo, hi], "#8b949e", linewidth=0.9, linestyle="--", zorder=2)
        ax.set_xlim(lo, hi); ax.set_ylim(lo, hi)
        ax.set_xlabel("Actual depth (m)")
        ax.set_ylabel("Predicted depth (m)")
        ax.set_title(f"{m}\nMAE = {mae:.4f} m    RMSE = {rmse:.4f} m", fontsize=9)
    fig.suptitle("Predicted vs Actual — LOOCV", color="#e6edf3", y=1.01, fontsize=11)
    _dark(fig, axes)
    plt.tight_layout()
    fig.savefig(out, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  Saved {out}")


# ── main ──────────────────────────────────────────────────────────────────────
def main():
    path = sys.argv[1] if len(sys.argv) > 1 else INPUT_FILE
    df  = load(path)
    agg = aggregate(df)

    print(f"\nInput file   : {path}")
    print(f"Raw samples  : {len(df)}")
    print(f"Stop clusters: {len(agg)}  "
          f"(ε={CLUSTER_EPS_M} m, min_size={MIN_CLUSTER_SZ})")
    print(f"Depth range  : {agg['z'].min():.3f} – {agg['z'].max():.3f} m\n")

    summary, detail, idx = cross_validate(agg)
    print(f"Common valid folds: {len(idx)}\n")

    # ── metric table ──
    w = 22
    hdr = f"{'Method':<{w}} {'Folds':>6} {'MAE (m)':>10} {'RMSE (m)':>10} {'Max|e| (m)':>12}"
    print(hdr)
    print("─" * len(hdr))
    for _, r in summary.sort_values("rmse").iterrows():
        print(f"{r['method']:<{w}} {r['n_folds']:>6} "
              f"{r['mae']:>10.4f} {r['rmse']:>10.4f} {r['max_err']:>12.4f}")

    best = summary.sort_values("rmse").iloc[0]
    print(f"\n  Best by RMSE → {best['method']}  "
          f"(RMSE={best['rmse']:.4f} m, MAE={best['mae']:.4f} m)\n")

    detail.to_csv("cv_results.csv", index=False)
    print("Saved cv_results.csv")

    print("Generating plots...")
    plot_errors(detail, idx)
    plot_scatter(detail)
    print("\nDone.")


if __name__ == "__main__":
    main()