#!/usr/bin/env bash
set -euo pipefail

image="${BENCH_IMAGE:-toyfs-bench}"
results_dir="${BENCH_RESULTS_DIR:-bench_results}"

mkdir -p "$results_dir"

docker build -f bench/Dockerfile -t "$image" .
docker run --rm \
    -v "$PWD/$results_dir:/results" \
    -e BENCH_MODELS="${BENCH_MODELS:-A B}" \
    -e BENCH_THREADS="${BENCH_THREADS:-1 2 4 8}" \
    -e BENCH_CALLBACKS="${BENCH_CALLBACKS:-cheap heavy}" \
    -e BENCH_REPEATS="${BENCH_REPEATS:-5}" \
    -e BENCH_SHAPE="${BENCH_SHAPE:-mixed}" \
    -e BENCH_FILES="${BENCH_FILES:-100000}" \
    "$image"

echo "Results written to $results_dir/"
