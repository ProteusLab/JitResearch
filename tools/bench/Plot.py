#!/usr/bin/env python3
"""
Plot.py — Visualize benchmark metrics data collected into CSV files

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
REGIME: bars
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Compare geomean metrics values across series of experiments for a fixed set of benchmarks.

  CSV data layout:
    <series-dir>/           <- one directory per series (compiler, config, ...)
      <benchmark>.csv       <- one CSV per benchmark; rows = repeated runs

  For each CSV, the bar height = geometric mean of the metric column over
  all rows. Error bars show standard deviation. Bars for the same
  benchmark are grouped side-by-side, one bar per series.

  Output: one SVG per metric, named <metric>.svg in output directory

  Example:
    python3 Plot.py --regime bars \
        --data llvm/ asmjit/ lightning/ \
        --metrics instructions cycles ipc \
        --out plots/

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
REGIME: graphs
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Show how dependent metrics scale as a base metric grows (workload scaling).

  CSV data layout:
    <series-dir>/           <- one directory per series
      <benchmark>/          <- one sub-directory per benchmark
        <size>.csv          <- one CSV per workload size; rows = repeated runs

  Each CSV produces one point on the line:
    X = geometric mean of --base-metric across all rows
    Y = geometric mean of the dependent metric across all rows

  Points are sorted by X and connected with a line. A std shaded band
  is drawn around each line. Each output SVG contains one subplot per
  benchmark, tiled up to 3 columns wide with a shared legend at the top.

  Output: one SVG per dependent metric, named <metric>_vs_<base>.svg in output
  directory

  Example:
    python plot_metrics.py --regime graphs \
        --data llvm/ asmjit/ lightning/ \
        --base-metric instructions \
        --metrics cycles ipc l1i-cache-misses \
        --out plots/
"""

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker


# ── shared helpers ────────────────────────────────────────────────────────────

def geomean(values: np.ndarray) -> float:
    """Geometric mean, ignoring non-positive and non-finite values."""
    v = values[np.isfinite(values) & (values > 0)]
    return float(np.exp(np.mean(np.log(v)))) if len(v) > 0 else float("nan")


def auto_scale(max_val: float) -> tuple[int, float]:
    """
    Return (exp3, divisor) for a human-readable axis scale.
    exp3 is the largest multiple of 3 such that max_val / 10^exp3 >= 1,
    clamped to [0, 12]  (covers 1 … 1E12).
    """
    if not np.isfinite(max_val) or max_val <= 0:
        return 0, 1.0
    exp3 = int(np.floor(np.log10(max_val) / 3) * 3)
    exp3 = max(0, min(exp3, 12))
    return exp3, float(10 ** exp3)


def axis_label(metric: str, exp3: int) -> str:
    return f"{metric}  [1E{exp3}]" if exp3 > 0 else metric


# Muted/earthy palette — desaturated, professional
_PALETTE = [
    "#5B7FA6",  # muted steel blue
    "#A0674B",  # terracotta
    "#5A8C6E",  # sage green
    "#8C7A4B",  # warm tan/khaki
    "#7B6A8C",  # dusty mauve
    "#4B7A7A",  # muted teal
    "#8C5A5A",  # dusty rose/brick
]

def palette(n: int) -> list[str]:
    return (_PALETTE * ((n // len(_PALETTE)) + 1))[:n]


# ── bars regime ───────────────────────────────────────────────────────────────

def bars_load(data_dirs: list[str]) -> dict[str, dict[str, pd.DataFrame]]:
    """series -> benchmark -> DataFrame (rows = runs)."""
    result: dict[str, dict[str, pd.DataFrame]] = {}
    for d in data_dirs:
        p = Path(d)
        if not p.is_dir():
            print(f"Warning: '{d}' is not a directory, skipping.", file=sys.stderr)
            continue
        benches: dict[str, pd.DataFrame] = {}
        for csv in sorted(p.glob("*.csv")):
            try:
                benches[csv.stem] = pd.read_csv(csv)
            except Exception as e:
                print(f"Warning: cannot read '{csv}': {e}", file=sys.stderr)
        if benches:
            result[p.name] = benches
        else:
            print(f"Warning: no valid CSV files found in '{d}'.", file=sys.stderr)
    return result


def bars_aggregate(
    series_data: dict[str, dict[str, pd.DataFrame]],
    metric: str,
) -> tuple[pd.DataFrame, pd.DataFrame]:
    """Return (geomeans_df, stdev_df) with index=benchmark, columns=series."""
    benches: set[str] = set()
    for bmap in series_data.values():
        for b, df in bmap.items():
            if metric in df.columns:
                benches.add(b)
    if not benches:
        return pd.DataFrame(), pd.DataFrame()

    series_names = list(series_data.keys())
    means_rows, std_rows = [], []
    for bench in sorted(benches):
        mr, sr = {"benchmark": bench}, {"benchmark": bench}
        for s in series_names:
            df = series_data.get(s, {}).get(bench)
            if df is not None and metric in df.columns:
                v = df[metric].to_numpy(dtype=float)
                mr[s] = geomean(v)
                sr[s] = float(np.std(v[np.isfinite(v)], ddof=1)) if v.size > 1 else 0.0
            else:
                mr[s] = sr[s] = float("nan")
        means_rows.append(mr)
        std_rows.append(sr)

    return (pd.DataFrame(means_rows).set_index("benchmark"),
            pd.DataFrame(std_rows).set_index("benchmark"))


def bars_plot(
    means: pd.DataFrame,
    stdev: pd.DataFrame,
    metric: str,
    out_dir: Path,
) -> None:
    series = means.columns.tolist()
    benchmarks = means.index.tolist()
    n_s, n_g = len(series), len(benchmarks)
    if n_g == 0 or n_s == 0:
        print(f"  No data to plot for '{metric}', skipping.")
        return

    all_vals = means.to_numpy(dtype=float)
    max_val = float(np.nanmax(all_vals)) if all_vals.size else 1.0
    exp3, divisor = auto_scale(max_val if np.isfinite(max_val) else 1.0)

    group_width = 0.8
    bar_width = group_width / n_s
    x = np.arange(n_g)
    colors = palette(n_s)

    fig, ax = plt.subplots(figsize=(max(6, n_g * 1.4 + 2), 5))

    for i, (s, color) in enumerate(zip(series, colors)):
        offsets = x - group_width / 2 + bar_width * (i + 0.5)
        vals = means[s].to_numpy(dtype=float)
        errs = stdev[s].to_numpy(dtype=float) / divisor if s in stdev.columns else None
        bars = ax.bar(
            offsets, vals / divisor, width=bar_width * 0.9,
            label=s, color=color, zorder=3,
            yerr=errs, capsize=3,
            error_kw=dict(elinewidth=1, ecolor="black", alpha=0.6, capthick=1),
        )
        for rect, v in zip(bars, vals):
            if np.isnan(v):
                ax.text(
                    rect.get_x() + rect.get_width() / 2, 0.01, "N/A",
                    ha="center", va="bottom", fontsize=7, color="grey",
                    transform=ax.get_xaxis_transform(),
                )

    ax.set_xticks(x)
    ax.set_xticklabels(benchmarks, rotation=15, ha="right", fontsize=9)
    ax.set_ylabel(axis_label(metric, exp3), fontsize=10)
    ax.set_title(metric, fontsize=12, fontweight="bold")
    ax.legend(fontsize=9, framealpha=0.7)
    ax.yaxis.set_major_formatter(ticker.FuncFormatter(lambda v, _: f"{v:g}"))
    ax.grid(axis="y", linestyle="--", alpha=0.4, zorder=0)
    ax.set_axisbelow(True)
    ax.spines[["top", "right"]].set_visible(False)

    plt.tight_layout()
    out_path = out_dir / f"{metric}.svg"
    fig.savefig(out_path, format="svg", bbox_inches="tight")
    plt.close(fig)
    print(f"  Saved: {out_path}")


def run_bars(args: argparse.Namespace, out_dir: Path) -> None:
    print("Loading data (bars regime)...")
    series_data = bars_load(args.data)
    if not series_data:
        sys.exit("Error: no valid data directories found.")
    print(f"Series: {list(series_data.keys())}")

    for metric in args.metrics:
        print(f"\nPlotting: {metric}")
        means, stdev = bars_aggregate(series_data, metric)
        if means.empty:
            print(f"  No data for '{metric}', skipping.")
            continue
        bars_plot(means, stdev, metric, out_dir)


# ── graphs regime ─────────────────────────────────────────────────────────────

# series -> benchmark -> [(x_mean, y_mean, y_std), ...] sorted by x
GraphData = dict[str, dict[str, list[tuple[float, float, float]]]]


def graphs_load(
    data_dirs: list[str],
    base_metric: str,
    dep_metric: str,
) -> GraphData:
    result: GraphData = {}
    for d in data_dirs:
        p = Path(d)
        if not p.is_dir():
            print(f"Warning: '{d}' is not a directory, skipping.", file=sys.stderr)
            continue
        bench_map: dict[str, list[tuple[float, float, float]]] = {}
        for bench_dir in sorted(p.iterdir()):
            if not bench_dir.is_dir():
                continue
            points: list[tuple[float, float, float]] = []
            for csv in sorted(bench_dir.glob("*.csv")):
                try:
                    df = pd.read_csv(csv)
                except Exception as e:
                    print(f"Warning: cannot read '{csv}': {e}", file=sys.stderr)
                    continue
                if base_metric not in df.columns:
                    print(f"Warning: base metric '{base_metric}' not in '{csv}', skipping.", file=sys.stderr)
                    continue
                if dep_metric not in df.columns:
                    print(f"Warning: dep metric '{dep_metric}' not in '{csv}', skipping.", file=sys.stderr)
                    continue
                x = geomean(df[base_metric].to_numpy(dtype=float))
                yv = df[dep_metric].to_numpy(dtype=float)
                yv = yv[np.isfinite(yv) & (yv > 0)]
                if not np.isfinite(x) or len(yv) == 0:
                    continue
                points.append((
                    x,
                    geomean(yv),
                    float(np.std(yv, ddof=1)) if len(yv) > 1 else 0.0,
                ))
            if points:
                bench_map[bench_dir.name] = sorted(points, key=lambda t: t[0])
        if bench_map:
            result[p.name] = bench_map
    return result


def graphs_plot(
    series_data: GraphData,
    base_metric: str,
    dep_metric: str,
    out_dir: Path,
) -> None:
    all_benches: set[str] = set()
    for bmap in series_data.values():
        all_benches.update(bmap.keys())
    # Should be non-empty for valid series_data
    assert(all_benches)

    benches = sorted(all_benches)
    series_names = list(series_data.keys())
    colors = palette(len(series_names))

    all_x = [p[0] for bmap in series_data.values() for pts in bmap.values() for p in pts]
    all_y = [p[1] for bmap in series_data.values() for pts in bmap.values() for p in pts]
    x_exp3, x_div = auto_scale(max(all_x, default=1.0))
    y_exp3, y_div = auto_scale(max(all_y, default=1.0))

    n_benches = len(benches)
    ncols = min(3, n_benches)
    nrows = int(np.ceil(n_benches / ncols))
    fig, axes = plt.subplots(
        nrows, ncols,
        figsize=(max(5 * ncols, 7), 4 * nrows + 1),
        squeeze=False,
    )

    legend_handles, legend_labels = [], []

    for idx, bench in enumerate(benches):
        row, col = divmod(idx, ncols)
        ax = axes[row][col]

        for s_idx, (series, color) in enumerate(zip(series_names, colors)):
            pts = series_data.get(series, {}).get(bench)
            if not pts:
                continue
            xs  = np.array([p[0] for p in pts]) / x_div
            ys  = np.array([p[1] for p in pts]) / y_div
            std = np.array([p[2] for p in pts]) / y_div

            line, = ax.plot(
                xs, ys, marker="o", color=color,
                linewidth=1.8, markersize=4, label=series, zorder=3,
            )
            ax.fill_between(xs, ys - std, ys + std,
                            color=color, alpha=0.15, zorder=2)
            if idx == 0:
                legend_handles.append(line)
                legend_labels.append(series)

        ax.set_title(bench, fontsize=10, fontweight="bold")
        ax.set_xlabel(axis_label(base_metric, x_exp3), fontsize=8)
        ax.set_ylabel(axis_label(dep_metric,  y_exp3), fontsize=8)
        ax.grid(True, linestyle="--", alpha=0.4, zorder=0)
        ax.set_axisbelow(True)
        ax.spines[["top", "right"]].set_visible(False)
        ax.tick_params(labelsize=8)
        ax.xaxis.set_major_formatter(ticker.FuncFormatter(lambda v, _: f"{v:g}"))
        ax.yaxis.set_major_formatter(ticker.FuncFormatter(lambda v, _: f"{v:g}"))

    for idx in range(n_benches, nrows * ncols):
        r, c = divmod(idx, ncols)
        axes[r][c].set_visible(False)

    fig.legend(
        legend_handles, legend_labels,
        loc="upper center", ncol=min(len(series_names), 5),
        fontsize=9, framealpha=0.8, bbox_to_anchor=(0.5, 1.0),
    )
    fig.suptitle(f"{dep_metric} vs {base_metric}",
                 fontsize=13, fontweight="bold", y=1.03)

    plt.tight_layout()
    out_path = out_dir / f"{dep_metric}_vs_{base_metric}.svg"
    fig.savefig(out_path, format="svg", bbox_inches="tight")
    plt.close(fig)
    print(f"  Saved: {out_path}")


def run_graphs(args: argparse.Namespace, out_dir: Path) -> None:
    if not args.base_metric:
        sys.exit("Error: --base-metric is required for --regime graphs.")
    print("Loading data (graphs regime)...")

    for dep_metric in args.metrics:
        print(f"\nPlotting: {dep_metric} vs {args.base_metric}")
        series_data = graphs_load(args.data, args.base_metric, dep_metric)
        if not series_data:
            print("  No valid data found, skipping.")
            continue
        graphs_plot(series_data, args.base_metric, dep_metric, out_dir)


# ── CLI ───────────────────────────────────────────────────────────────────────

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    p.add_argument(
        "--regime", required=True, choices=["bars", "graphs"],
        help=(
            "Plotting regime (required).\n"
            "\n"
            "  bars   — Grouped bar chart comparing absolute metric values\n"
            "           across series for a fixed set of benchmarks.\n"
            "           Expected layout inside each --data dir:\n"
            "             <series-dir>/<benchmark>.csv\n"
            "           Bar height = geomean over repeated runs (CSV rows).\n"
            "           Error bars = +/-1 standard deviation.\n"
            "           Bars for the same benchmark are placed side-by-side.\n"
            "           Output: one SVG per metric, named <metric>.svg\n"
            "\n"
            "  graphs — Line chart showing how a dependent metric scales\n"
            "           with a base metric as workload size grows.\n"
            "           Expected layout inside each --data dir:\n"
            "             <series-dir>/<benchmark>/<workload_size>.csv\n"
            "           X = geomean of --base-metric across all rows of a CSV.\n"
            "           Y = geomean of the dependent metric across same rows.\n"
            "           Points sorted by X and connected with a line;\n"
            "           shaded band shows +/-1 std around each line.\n"
            "           One subplot per benchmark, tiled in the same figure.\n"
            "           Output: one SVG per metric, named <metric>_vs_<base-metric>.svg\n"
        ),
    )
    p.add_argument(
        "--data", nargs="+", required=True, metavar="DIR",
        help=(
            "One or more series directories. The directory name is used as\n"
            "the legend label. See --regime for the expected layout inside\n"
            "each directory."
        ),
    )
    p.add_argument(
        "--metrics", nargs="+", required=True, metavar="METRIC",
        help=(
            "One or more CSV column names to plot.\n"
            "bars regime   : these are the Y-axis metrics, one chart each.\n"
            "graphs regime : these are the dependent (Y-axis) metrics;\n"
            "                the X axis is controlled by --base-metric."
        ),
    )
    p.add_argument(
        "--base-metric", metavar="METRIC", default=None,
        help=(
            "Column name to use as the X axis in 'graphs' regime.\n"
            "Its geomean across all rows of a CSV represents the workload\n"
            "size for that data point. Required for --regime graphs,\n"
            "ignored for --regime bars."
        ),
    )
    p.add_argument(
        "--out", default="plots", metavar="DIR",
        help="Output directory for SVG files (default: ./plots).",
    )

    return p.parse_args()


def main() -> None:
    args = parse_args()
    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.regime == "bars":
        run_bars(args, out_dir)
    else:
        run_graphs(args, out_dir)

    print("\nDone.")


if __name__ == "__main__":
    main()
