#!/usr/bin/env bash
set -euo pipefail

shape="${BENCH_SHAPE:-mixed}"
files="${BENCH_FILES:-100000}"
out="${BENCH_OUT:-/results/results.csv}"
meta="${BENCH_META:-/results/metadata.txt}"

mkdir -p "$(dirname "$out")"
mkdir -p "$(dirname "$meta")"

fixture="$(mktemp -d "/tmp/toyfs-bench-${shape}.XXXXXX")"

{
    printf 'timestamp_utc='
    date -u '+%Y-%m-%dT%H:%M:%SZ'
    printf 'uname='
    uname -a
    printf 'nproc='
    nproc
    printf 'cpu_model='
    grep -m 1 'model name' /proc/cpuinfo | cut -d ':' -f 2- | sed 's/^ //'
    printf 'page_size='
    getconf PAGESIZE
    printf 'bench_shape=%s\n' "$shape"
    printf 'bench_files=%s\n' "$files"
    env | grep '^BENCH_' | sort
} >"$meta"

bench/make_fixture.sh "$fixture" "$shape" "$files"
bench/run_bench.sh "$fixture" "$out" "$shape"

echo "Benchmark metadata: $meta"
echo "Benchmark results:  $out"
