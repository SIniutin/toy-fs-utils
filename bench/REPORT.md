# Crawler Architecture Benchmark Report

This report summarizes the first benchmark pass comparing two crawler
architectures:

- `model=A`: split pool, with traversal workers feeding a processing queue.
- `model=B`: one pool, where each worker both traverses and runs the callback.

The goal is not to claim universal performance numbers. The goal is to
understand architectural trade-offs under controlled, repeatable workloads.

## Methodology

The benchmark was run through the Docker benchmark flow:

```bash
BENCH_MATRIX_DIR=bench_results_matrix_20k \
BENCH_FILES=20000 \
BENCH_REPEATS=5 \
BENCH_THREADS="1 2 4 6 8 12 16" \
BENCH_CALLBACKS="cheap stat heavy" \
BENCH_MODELS="A B" \
bench/run_docker_matrix.sh
```

Each parameter combination performs one warmup run and five measured runs.
Reported values are medians.

## Environment

```text
container: ubuntu:24.04
kernel: Linux 7.1.4-1-MANJARO
cpu: 12th Gen Intel(R) Core(TM) i5-12450H
nproc: 12
page_size: 4096
```

## Workloads

Tree shapes:

- `wide`: files spread across a small number of wide directories.
- `mixed`: files spread across a two-level directory tree.
- `deep`: files under a deep directory chain.

Callback modes:

- `cheap`: atomic counters only.
- `stat`: an additional `lstat` per entry.
- `heavy`: CPU-heavy path hashing loop.

## Best Results

| tree | callback | best model | threads | wall_ms | throughput |
|---|---|---:|---:|---:|---:|
| wide | cheap | B | 4 | 13.380 | 1,497,136.7 |
| wide | stat | B | 16 | 10.221 | 1,959,864.5 |
| wide | heavy | B | 12 | 150.985 | 132,675.4 |
| mixed | cheap | B | 16 | 12.282 | 1,640,059.8 |
| mixed | stat | B | 12 | 13.473 | 1,495,153.0 |
| mixed | heavy | B | 16 | 156.780 | 128,486.0 |
| deep | cheap | B | 12 | 22.221 | 909,282.9 |
| deep | stat | B | 8 | 33.009 | 612,103.0 |
| deep | heavy | A | 16 | 262.956 | 76,837.8 |

## Key Findings

`model=B` is the better default architecture.

For cheap and stat callbacks, the one-pool design wins almost everywhere. It
avoids sending every entry through a second queue, so it pays much less
synchronization overhead.

`model=A` has high queue handoff overhead.

On cheap callbacks, `model=A` produces dramatically more voluntary context
switches. For example, on `mixed/cheap` with 16 threads:

| model | wall_ms | vol_cs |
|---|---:|---:|
| A | 43.874 | 22,775 |
| B | 12.282 | 562 |

That is the clearest architectural cost of the split-pool design: when per-entry
work is cheap, queue handoff dominates useful work.

`model=B` generally saturates around 8-12 threads.

The machine reports `nproc=12`, and most `model=B` curves flatten or regress in
the 8-16 thread range. Extra workers help until the crawler has enough parallel
work to keep cores busy; after that, scheduling and synchronization start to
eat the gains.

`model=A` wins one important special case: `deep/heavy`.

On the deep tree with heavy callbacks:

| model | threads | wall_ms | cpu_util | throughput |
|---|---:|---:|---:|---:|
| A | 16 | 262.956 | 7.89 | 76,837.8 |
| B | 16 | 524.007 | 3.64 | 38,558.6 |

This suggests a traversal-frontier bottleneck. In `model=B`, workers that find
entries immediately spend time in heavy callbacks, which can delay discovery of
more directory work. In `model=A`, traversal and callback execution are
decoupled, so processing workers can consume heavy work while traversal workers
continue exposing new tasks.

## Architectural Takeaway

More pools are not automatically faster.

The split-pool design is useful when callback work is expensive enough that
decoupling traversal from processing keeps the machine busier. For general
filesystem utilities with cheap or moderate callbacks, the extra queue, wakeups,
and context switches are a net loss.

Current recommendation:

- keep `model=B` as the production crawler architecture;
- keep `model=A` as benchmark/research code;
- consider a split-pool variant only for future workloads with expensive
  processing stages or explicit traversal/processing isolation needs.

## Limitations

- Results are from one machine and one Docker runtime.
- Filesystem cache effects still matter even inside Docker.
- The benchmark uses generated synthetic trees, not real project trees.
- `model=A` is benchmark-only code and has not been production-hardened.
- Queue contention is inferred from context switches, not instrumented directly.

## Next Steps

- Add optional queue instrumentation for blocked push/pop counts and wait time.
- Run a larger matrix, for example 100k or 1M files, after the benchmark tooling
  remains stable.
- Add a real-world fixture based on a copied source tree.
- Save generated plots from the notebook for easier review.
