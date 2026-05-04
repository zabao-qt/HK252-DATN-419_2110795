from __future__ import annotations
import sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy.interpolate import Rbf
from scipy.spatial import cKDTree, Delaunay

INPUT_FILE     = "data_clean.txt"
GRID_N         = 120
CLUSTER_EPS_M  = 3.0
MIN_CLUSTER_SZ = 3
KPA2MM         = 1000.0 / 9.80665
Z_EXAG         = 10.0


def load(path):
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


def total_depth_m(kpa, mm):
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
            k = stack.pop()
            comp.append(k)
            for j in tree.query_ball_point(pts[k], eps):
                if not visited[j]:
                    visited[j] = True
                    stack.append(j)
        comps.append(comp)
    return comps


def aggregate(df):
    x, y, lat0, lon0 = to_meters(df["lat"], df["lon"])
    z = total_depth_m(df["kpa"], df["mm"])
    rows = []
    for comp in cluster_components(x, y, CLUSTER_EPS_M):
        xi, yi, zi = x[comp], y[comp], z[comp]
        ts = df["ts"].to_numpy(float)[comp]
        if len(comp) >= MIN_CLUSTER_SZ:
            rows.append(dict(x=float(np.median(xi)),
                             y=float(np.median(yi)),
                             z=float(np.median(zi)),
                             ts=float(np.median(ts))))
        else:
            for i in range(len(comp)):
                rows.append(dict(x=float(xi[i]), y=float(yi[i]),
                                 z=float(zi[i]),  ts=float(ts[i])))
    agg = pd.DataFrame(rows).sort_values("ts").reset_index(drop=True)
    return agg, x, y, z, lat0, lon0


def interpolate_tps(agg):
    px = agg["x"].to_numpy(float)
    py = agg["y"].to_numpy(float)
    pz = agg["z"].to_numpy(float)

    # collapse exact duplicates
    df_u = pd.DataFrame({"x": px.round(6), "y": py.round(6), "z": pz})
    df_u = df_u.groupby(["x", "y"], as_index=False)["z"].median()
    px = df_u["x"].to_numpy(float)
    py = df_u["y"].to_numpy(float)
    pz = df_u["z"].to_numpy(float)

    rbf = Rbf(px, py, pz, function="thin_plate", smooth=0.0)

    xg = np.linspace(px.min(), px.max(), GRID_N)
    yg = np.linspace(py.min(), py.max(), GRID_N)
    X, Y = np.meshgrid(xg, yg)
    Z    = rbf(X, Y)

    # mask outside convex hull
    hull   = Delaunay(np.column_stack([px, py]))
    inside = hull.find_simplex(
        np.column_stack([X.ravel(), Y.ravel()])) >= 0
    Z = np.ma.array(Z, mask=~inside.reshape(X.shape))

    return X, Y, Z


BG   = "#0d1117"
AX   = "#161b22"
EDGE = "#30363d"
TICK = "#8b949e"
TXT  = "#c9d1d9"
HEAD = "#e6edf3"


def _dark2d(fig, ax):
    fig.patch.set_facecolor(BG)
    ax.set_facecolor(AX)
    for sp in ax.spines.values():
        sp.set_edgecolor(EDGE)
    ax.tick_params(colors=TICK)
    ax.xaxis.label.set_color(TXT)
    ax.yaxis.label.set_color(TXT)
    ax.title.set_color(HEAD)


def plot_2d(X, Y, Z, raw_x, raw_y, agg, out="depth_2d.png"):
    fig, ax = plt.subplots(figsize=(9, 8))

    cf = ax.pcolormesh(X, Y, Z, shading="auto", cmap="viridis_r")
    cbar = plt.colorbar(cf, ax=ax, pad=0.02)
    cbar.set_label("Total depth  (m)", color=TXT)
    cbar.ax.yaxis.set_tick_params(color=TICK)
    plt.setp(cbar.ax.yaxis.get_ticklabels(), color=TICK)

    ax.plot(raw_x, raw_y, color="white", linewidth=0.7,
            alpha=0.30, zorder=2, label="Survey track")

    ax.scatter(agg["x"], agg["y"], c=agg["z"], cmap="viridis_r",
               s=60, edgecolors="white", linewidths=0.5, zorder=3,
               vmin=float(Z.min()), vmax=float(Z.max()),
               label="Measurement stops")

    ax.set_xlabel("Easting  (m)")
    ax.set_ylabel("Northing  (m)")
    ax.set_title("Bathymetric depth map — thin-plate spline")
    ax.set_aspect("equal")
    leg = ax.legend(facecolor="#21262d", labelcolor=TXT, framealpha=0.8)
    _dark2d(fig, ax)

    plt.tight_layout()
    fig.savefig(out, dpi=180, bbox_inches="tight")
    print(f"  Saved {out}")
    return fig


def plot_3d(X, Y, Z, agg, out="depth_3d.png"):
    fig = plt.figure(figsize=(11, 8))
    fig.patch.set_facecolor(BG)
    ax  = fig.add_subplot(111, projection="3d")
    ax.set_facecolor(BG)

    Zplot = -Z  # negate: deeper
    ax.plot_surface(X, Y, Zplot * Z_EXAG,
                    cmap="viridis", alpha=0.88,
                    linewidth=0, antialiased=True)

    ax.scatter(agg["x"], agg["y"], -agg["z"] * Z_EXAG,
               c="white", s=20, depthshade=False, zorder=5)

    xr = float(X.max() - X.min())
    yr = float(Y.max() - Y.min())
    zr = float(np.nanmax(np.abs(Z.data)) - np.nanmin(np.abs(Z.data)))
    ax.set_box_aspect((xr, yr, zr * Z_EXAG * 2.0))

    ax.set_xlabel("Easting  (m)",  color=TXT, labelpad=8)
    ax.set_ylabel("Northing  (m)", color=TXT, labelpad=8)
    ax.set_zlabel(f"−Depth  (m, {Z_EXAG:.0f}× V.E.)", color=TXT, labelpad=8)
    ax.set_title(f"3-D lakebed surface — thin-plate spline\n"
                 f"({Z_EXAG:.0f}× vertical exaggeration)",
                 color=HEAD)
    ax.tick_params(colors=TICK)

    plt.tight_layout()
    fig.savefig(out, dpi=180, bbox_inches="tight")
    print(f"  Saved {out}")
    return fig


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else INPUT_FILE
    df = load(path)
    if df.empty:
        raise ValueError(f"No valid data in {path}")

    agg, raw_x, raw_y, raw_z, lat0, lon0 = aggregate(df)
    print(f"\nLoaded       : {len(df)} raw samples → {len(agg)} aggregated stops")
    print(f"Centre       : lat {lat0:.6f}  lon {lon0:.6f}")
    print(f"Depth range  : {agg['z'].min():.3f} – {agg['z'].max():.3f} m\n")

    print("Interpolating...")
    X, Y, Z = interpolate_tps(agg)
    print(f"Grid: {X.shape[1]}×{X.shape[0]}  "
          f"depth range: {float(Z.min()):.3f} – {float(Z.max()):.3f} m\n")

    print("Generating plots...")
    plot_2d(X, Y, Z, raw_x, raw_y, agg)
    fig3d = plot_3d(X, Y, Z, agg)
    print("\nDone. Close the 3D window to exit.")
    plt.show()


if __name__ == "__main__":
    main()