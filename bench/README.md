# Crawler Benchmarks

This directory contains opt-in benchmarks for the crawler architecture. These
are performance experiments, not correctness tests, so they are not part of the
default `./build.sh` or CTest flow.

## Docker Run

The preferred path is the Docker runner. It builds the benchmark binary in a
Release container, creates the fixture inside the container, writes raw CSV
results, and records environment metadata.

```bash
bench/run_docker_bench.sh
```

Results are written to:

```text
bench_results/results.csv
bench_results/metadata.txt
```

Environment knobs:

```bash
BENCH_MODELS="A B" \
BENCH_THREADS="1 2 4 8 16" \
BENCH_CALLBACKS="cheap stat heavy" \
BENCH_REPEATS=7 \
BENCH_SHAPE=mixed \
BENCH_FILES=100000 \
bench/run_docker_bench.sh
```

For a broader exploratory matrix across tree shapes:

```bash
BENCH_FILES=20000 \
BENCH_REPEATS=5 \
BENCH_THREADS="1 2 4 6 8 12 16" \
BENCH_CALLBACKS="cheap stat heavy" \
BENCH_MODELS="A B" \
bench/run_docker_matrix.sh
```

This writes:

```text
bench_results_matrix/<shape>/results.csv
bench_results_matrix/<shape>/metadata.txt
bench_results_matrix/summary.md
```

Docker gives a cleaner build/runtime baseline, but it does not make results
perfectly hardware-independent. CPU model, kernel, Docker runtime, host load,
and storage/cache behavior still matter, so `metadata.txt` is saved with every
run.

## Local Build

Local runs are useful for fast smoke tests while changing benchmark code. Use a
release build:

```bash
cmake -S . -B build_bench -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
cmake --build build_bench
```

## Fixture

Create a fresh fixture directory:

```bash
bench/make_fixture.sh /tmp/toyfs-bench-mixed mixed 100000
```

Supported shapes:

- `wide` - files spread across a small number of wide directories.
- `deep` - files under a deep directory chain.
- `mixed` - files spread across a two-level directory tree.

The fixture script refuses to write into a non-empty directory.

## Run

```bash
bench/run_bench.sh /tmp/toyfs-bench-mixed results.csv mixed
```

The runner performs one warmup run for each parameter combination, then records
measured runs as raw CSV rows.

Local runner knobs:

```bash
BENCH_MODELS="A B" BENCH_THREADS="1 2 4 8 16" BENCH_CALLBACKS="cheap stat heavy" BENCH_REPEATS=7 \
    bench/run_bench.sh /tmp/toyfs-bench-mixed results.csv mixed
```

`model=A` needs at least two threads, so the runner skips `A` when
`BENCH_THREADS` contains `1`.

## Models

`model=B` is the current crawler architecture:

```text
one pool:
  worker -> readdir/stat -> callback
```

`model=A` is a benchmark-only split-pool architecture:

```text
traversal pool:
  readdir/stat
  directory task -> traversal queue
  file task      -> processing queue

processing pool:
  callback
```

For `model=A`, `--threads N` is the total worker budget. The benchmark splits it
between traversal workers and processing workers instead of creating `N + N`
threads.

## CSV Fields

```csv
run,model,tree,callback,threads,entries,wall_ms,cpu_ms,cpu_util,throughput,vol_cs,invol_cs,bytes,checksum,rc
```

Fields:

- `wall_ms` - elapsed wall-clock time.
- `cpu_ms` - process CPU time.
- `cpu_util` - `cpu_ms / wall_ms`.
- `throughput` - processed entries per second.
- `vol_cs` / `invol_cs` - voluntary and involuntary context switches from `getrusage`.
- `rc` - crawler return code.

## Summarize

```bash
bench/summarize_results.py bench_results_matrix/*/results.csv > bench_results_matrix/summary.md
```

The summary groups by tree shape, callback mode, model, and thread count. It
uses median values and reports speedup relative to the lowest thread count
available for that model.

## Notebook

For visual analysis:

```bash
jupyter notebook bench/crawler_benchmark_analysis.ipynb
```

The notebook reads `../bench_results_matrix_20k` by default when opened from the
`bench/` directory. Change `RESULTS_DIR` in the first code cell if your matrix
directory has another name.

Python requirements:

```text
matplotlib
```

Install them into the active Jupyter kernel:

```python
%pip install -r bench/requirements.txt
```

If the notebook is opened from the `bench/` directory instead of the repository
root, use:

```python
%pip install -r requirements.txt
```

## Methodology Notes

- Run benchmarks on a quiet machine.
- Use release builds only.
- Keep fixture shape and size fixed when comparing models.
- Compare medians from repeated runs, not a single best run.
- Avoid mixing cold-cache and warm-cache results in the same comparison.
