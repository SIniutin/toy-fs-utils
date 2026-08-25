#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 ROOT OUT_CSV [TREE_NAME]" >&2
}

root="${1:-}"
out="${2:-}"
tree="${3:-unknown}"
bench_bin="${BENCH_BIN:-./build_bench/bin/bench_crawler}"
models="${BENCH_MODELS:-B}"
threads="${BENCH_THREADS:-1 2 4 8}"
callbacks="${BENCH_CALLBACKS:-cheap heavy}"
repeats="${BENCH_REPEATS:-5}"

if [ -z "$root" ] || [ -z "$out" ]; then
    usage
    exit 1
fi

if [ ! -x "$bench_bin" ]; then
    echo "Benchmark binary not found or not executable: $bench_bin" >&2
    echo "Build with: cmake -S . -B build_bench -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON && cmake --build build_bench" >&2
    exit 1
fi

echo "run,model,tree,callback,threads,entries,wall_ms,cpu_ms,cpu_util,throughput,vol_cs,invol_cs,bytes,checksum,rc" >"$out"

for model in $models; do
    for callback in $callbacks; do
        for thread_count in $threads; do
            if [ "$model" = "A" ] && [ "$thread_count" -lt 2 ]; then
                continue
            fi

            "$bench_bin" --root "$root" --model "$model" --tree "$tree" --callback "$callback" --threads "$thread_count" >/dev/null

            for ((run = 1; run <= repeats; ++run)); do
                row="$("$bench_bin" --root "$root" --model "$model" --tree "$tree" --callback "$callback" --threads "$thread_count")"
                echo "$run,$row" >>"$out"
            done
        done
    done
done

echo "Wrote benchmark results to $out"
