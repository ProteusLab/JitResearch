import statistics
import seaborn as sns
import matplotlib.pyplot as plt
import pandas as pd
import sys
from itertools import repeat
from tqdm.contrib.concurrent import thread_map
import numpy as np
import argparse
from pathlib import Path
import Benchmark


class BenchmarkList:
    def __init__(self, data: list[Benchmark.BenchmarkData], name: str):
        self.data = data
        self.name = name

    def name(self):
        return self.name

    def data(self):
        return self.data

    def mips(self):
        return statistics.geometric_mean([x.mips() for x in self.data])


class HistData:
    def __init__(self, bench: str, mips: float, backend: str):
        self.bench = bench
        self.mips = mips
        self.backend = backend


def build_hist(list_data: list[list[BenchmarkList]], bench_names: list[str]):
    num_groups = len(list_data)
    num_bars_per_group = len(list_data[0]) if num_groups > 0 else 0

    data = {"Benchmark": [], "MIPS": [], "Backend": []}

    for j in range(num_bars_per_group):
        for i in range(num_groups):
            data["Benchmark"].append(bench_names[i])
            data["MIPS"].append(list_data[i][j].mips())
            data["Backend"].append(list_data[i][j].name)

    df = pd.DataFrame(data)
    print(df)
    filtered_df = df[~df["Backend"].isin(["nopbench"])]
    avg_df = (
        filtered_df.groupby("Backend")
        .agg(
            MIPS=("MIPS", statistics.geometric_mean),
        )
        .reset_index()
        .assign(Benchmark="geomean")
    )

    final_df = pd.concat([df, avg_df], ignore_index=True)
    df = final_df

    sns.set(style="whitegrid")

    plt.figure(figsize=(12, 8))
    bar_plot = sns.barplot(
        x="Benchmark",
        y="MIPS",
        hue="Backend",
        data=df,
        palette=sns.color_palette(),
    )
    plt.ylabel("MIPS")
    plt.legend(
        title="Backends",
        loc="upper right",
        fontsize="small",
        title_fontsize="medium",
    )
    sns.move_legend(bar_plot, "upper left", bbox_to_anchor=(1, 1))
    plt.tight_layout()
    plt.savefig("hist.png")
    plt.close()


def run_benchmark(request: Benchmark.BenchmarkRequest, times: int):
    bench = Benchmark.Benchmark(request)
    results = []
    for _ in range(times):
        bench.run()
        exit_result = bench.exit()
        exit_result.assert_success()
        results.append(exit_result.benchmark_data())
    return BenchmarkList(results, request.jit_name)


def run_elf(elf, th_num: int, times: int):
    requests = [Benchmark.BenchmarkRequest(elf)]
    jits = ["cached-interp", "llvm", "xbyak", "asmjit", "lightning", "mir"]
    for jit in jits:
        requests.append(Benchmark.BenchmarkRequest(elf, th_num, jit))
    data = []
    print("Running: " + benchmark_name(elf))

    for req in thread_map(run_benchmark, requests, repeat(times)):
        data.append(req)
    return data, benchmark_name(elf)


def run_benchmarks(elf_paths: list[str], threshold_num: int, times: int):
    list_data = []
    bench_names = []
    for data, name in map(
        run_elf, elf_paths, repeat(threshold_num), repeat(times)
    ):
        list_data.append(data)
        bench_names.append(name)
    build_hist(list_data, bench_names)


def benchmark_name(elf_path: Path):
    return elf_path.name


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Tool for benchmarking")
    parser.add_argument("sim_bin", type=Path, help="Path to sim binary")
    parser.add_argument(
        "elf", type=Path, nargs="+", help="Path(s) to benchmarks"
    )
    parser.add_argument(
        "-t",
        "--times",
        type=int,
        default=2,
        help="Amount of times to run each bench",
    )
    parser.add_argument("--thres", type=int, default=0, help="JIT threshold")

    args = parser.parse_args()

    Benchmark.sim_exe_path = args.sim_bin
    run_benchmarks(args.elf, args.thres, args.times)
