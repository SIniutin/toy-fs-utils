#!/usr/bin/env bash
set -euo pipefail

results_root="${BENCH_MATRIX_DIR:-bench_results_matrix}"
shapes="${BENCH_SHAPES:-wide mixed deep}"

mkdir -p "$results_root"

csvs=()
for shape in $shapes; do
    shape_dir="$results_root/$shape"
    BENCH_RESULTS_DIR="$shape_dir" BENCH_SHAPE="$shape" bench/run_docker_bench.sh
    csvs+=("$shape_dir/results.csv")
done

bench/summarize_results.py "${csvs[@]}" >"$results_root/summary.md"
echo "Matrix summary written to $results_root/summary.md"
