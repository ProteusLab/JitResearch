# CLAUDE_Plot.md — Benchmark Metrics Plotter

## Project overview

`Plot.py` — a single Python script for visu alising benchmark performance
data collected across multiple series (e.g. JIT compilers, build configs).
Produces SVG charts. Two regimes selectable via `--regime`.

## Usage

```bash
# Bars: compare absolute metric values across series
python3 Plot.py \
    --regime bars \
    --data results/llvm results/asmjit results/gcc \
    --metrics cycles ipc l1i-cache-misses \
    --out plots/

# Graphs: show how metrics scale with a base metric (workload scaling)
python3 Plot.py \
    --regime graphs \
    --data results/llvm results/asmjit results/gcc \
    --base-metric instructions \
    --metrics cycles ipc \
    --out plots/
```

## CLI arguments

| Argument | Required | Description |
|---|---|---|
| `--regime` | yes | `bars` or `graphs` |
| `--data` | yes | One or more series directories; dir name = legend label |
| `--metrics` | yes | CSV column names to plot |
| `--base-metric` | graphs only | Column used as X axis (workload proxy) |
| `--out` | no | Output directory for SVGs (default: `./plots`) |
| `--logx` | no | Logarithmic X axis (graphs regime only) |
| `--logy` | no | Logarithmic Y axis (both regimes) |

## Data layouts

### `bars` regime

```
<series-dir>/
    <benchmark>.csv     ← rows = independent repeated runs
```

- Benchmarks matched across series by filename (without extension)
- One SVG per metric, named `<metric>.svg`

### `graphs` regime

```
<series-dir>/
    <benchmark>/
        <workload_size>.csv   ← rows = independent repeated runs at that size
```

- Benchmarks matched across series by sub-directory name
- One SVG per dependent metric, named `<metric>_vs_<base-metric>.svg`

## CSV format

Plain CSV with a header row. Column names are metric names. Each data row is
one independent run. Example:

```
instructions,cycles,branches,l1i-cache-misses
100,50,10,1
110,50,11,2
```

## How values are computed

- **Bar height / line Y point** — geometric mean of all rows in a CSV file
- **Error bars / shaded band** — ±1 standard deviation across rows
- **Line X point** (`graphs` only) — geometric mean of `--base-metric` in that CSV

Geomean is used rather than arithmetic mean because it is more appropriate for
ratios and hardware counters that span orders of magnitude. Non-positive and
non-finite values are excluded before computing geomean (required since
`log(x)` is undefined for `x ≤ 0`).

## Axis formatting

On linear axes, matplotlib's `ScalarFormatter` is used with `useMathText=True`
and `powerlimits=(-3, 3)`, which produces an automatic `×10^N` offset label
when values are large or small. The axis label shows only the bare metric name.

On log axes (`--logx` / `--logy`), matplotlib's default `LogFormatter` is used,
showing ticks in `10^x` notation. `nonpositive="clip"` is passed to
`set_xscale`/`set_yscale` so that bars starting from 0 and any negative
error-bar lower bounds are silently clipped to a small positive value rather
than crashing or producing invisible elements.

## Dependencies

```bash
pip install numpy pandas matplotlib
```

## Known issues / notes

- In `graphs_plot`, the emptiness check on `all_benches` is now an `assert`
  rather than a soft print-and-return, reflecting that an empty `series_data`
  should be caught upstream in `run_graphs` and never reach this point.
- `--base-metric` is silently ignored when `--regime bars` is used.
- Series with no valid CSV files produce a warning and are skipped rather than
  causing a hard error.
- In `graphs` regime, CSV files missing the dependent metric column produce a
  warning and are skipped (the base metric missing is also warned).
- Missing benchmark/series combinations in `bars` regime show an "N/A"
  annotation on the bar rather than crashing.
