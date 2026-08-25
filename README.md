# Toy-fs-utils

A collection of small utilities for Linux/Unix systems, written in pure C.
The project centers on a reusable pthread-based directory crawler and the
filesystem tools built around it.

## Motivation

File system traversal looks trivial until it needs to be fast, scalable, and safe.
This project explores concurrent crawling, explicit queue/cancellation contracts,
and practical POSIX filesystem workflows.

## Utilities

### diskusage

Concurrent disk usage analyzer with an emphasis on directory traversal behavior.

### trashbin

Safe file removal utilities that move files to a trash directory instead of
deleting them permanently.

Features:
- `rm_trash` moves files into `~/.trash` and records metadata in `~/.trash.log`.
- `list_trash` lists recorded trash entries.
- `untrash` restores matching entries.

### fswatch

Inotify-based filesystem watcher for tracking changes in directories and files.

### backup

Simple backup utilities for copying, listing, and restoring directory backups.

Features:
- `backup` creates or updates backups under `~/Backups`.
- `backup_list` lists available backups.
- `upback` restores a backup into the original source path or a custom directory.

## Directory Crawler

The directory crawler is the core component of the project. It traverses
directory trees with a bounded task queue, worker threads, active-task tracking,
and callback-driven processing.

Key aspects:
- Bounded ring-buffer task queue
- Graceful close/cancel behavior
- Worker lifecycle based on active directory tasks
- Callback-driven traversal control and error propagation

## Build

```bash
./build.sh
```

Built binaries are written to `build/bin/`.

## Tests

The repository uses CTest for unit and end-to-end tests. Tests run against
temporary directories and temporary `$HOME` values.

```bash
ctest --test-dir build --output-on-failure
```

Docker can be used for a clean Linux test environment:

```bash
docker build -f tests/Dockerfile -t toyfs-tests .
docker run --rm toyfs-tests
```

Coverage is measured with gcov in a separate coverage build. The current
source-only line coverage target is around 80%.

## Documentation

User-facing utility documentation lives in [`docs/`](docs/README.md).

## Benchmarks

Opt-in crawler benchmarks live in [`bench/`](bench/README.md). They are built
separately from the default test flow:

```bash
cmake -S . -B build_bench -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
cmake --build build_bench
```

Current measurements include:
- Total traversal time
- Throughput
- CPU utilization
- Context switches
- Scalability across worker counts

Sample result from the 20k-file Docker matrix on an Intel i5-12450H:

| workload | best model | threads | wall time | throughput |
|---|---:|---:|---:|---:|
| `wide / cheap` | B | 4 | 13.380 ms | 1,497,136 files/s |
| `mixed / stat` | B | 12 | 13.473 ms | 1,495,153 files/s |
| `mixed / heavy` | B | 16 | 156.780 ms | 128,486 files/s |
| `deep / heavy` | A | 16 | 262.956 ms | 76,838 files/s |

The benchmark report is in [`bench/REPORT.md`](bench/REPORT.md).
