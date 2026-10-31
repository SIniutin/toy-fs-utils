# Toy-fs-utils

[![CI](https://github.com/SIniutin/toy-fs-utils/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/SIniutin/toy-fs-utils/actions/workflows/ci.yml)
[![C17](https://img.shields.io/badge/C-17-blue.svg)](https://en.cppreference.com/w/c/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A collection of small utilities for Linux/Unix systems, written in pure C.
The project centers on a reusable pthread-based directory crawler and the
filesystem tools built around it.

## Motivation

File system traversal looks trivial until it needs to be fast, scalable, and safe.
This project explores concurrent crawling, explicit queue/cancellation contracts,
and practical POSIX filesystem workflows.

## Utilities

| Utility | Purpose | Main interfaces | Platform | Status |
|---|---|---|---|---|
| `diskusage` | Concurrent directory size and top-file report | `pthread`, `opendir`, `lstat` | Linux, macOS | Stable |
| `rm_trash` | Move files into a recoverable per-user trash | hard links, `unlink`, recursive crawler | Linux, macOS | Stable |
| `list_trash` | Filter and sort recorded trash entries | metadata log parsing | Linux, macOS | Stable |
| `untrash` | Restore trashed files with conflict handling | `link`, `rename`, path validation | Linux, macOS | Stable |
| `fswatch` | Stream filesystem events | `inotify` | Linux only | Stable |
| `backup` | Create incremental snapshots and versions | crawler, `stat`, atomic metadata writes | Linux, macOS | Stable |
| `backup_list` | Inspect available backups and versions | directory traversal, metadata parsing | Linux, macOS | Stable |
| `upback` | Restore a backup to its source or another directory | `nftw`, validated relative paths | Linux, macOS | Stable |

The macOS build intentionally excludes `fswatch`, because it is implemented
against Linux `inotify`. The remaining utilities use POSIX APIs, and the CI
workflow includes a macOS build/test job for that portable subset. Linux is the
primary development platform and the only platform that builds the complete
tool set.

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

Install the utilities under a chosen prefix:

```bash
cmake --install build --prefix "$HOME/.local"
```

Make sure `$HOME/.local/bin` is in `PATH`. CMake records installed files in the
build directory, so the same build can remove exactly those files:

```bash
cmake --build build --target uninstall
```

The project version is defined in CMake. Release tags follow `vMAJOR.MINOR.PATCH`.

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

## Portability

- **Linux:** complete build, including the `inotify`-based `fswatch` utility.
- **macOS:** portable crawler, disk-usage, trash, and backup utilities; no
  `fswatch` binary.
- **Windows:** not supported. The code relies on POSIX filesystem and pthread
  interfaces.

## License

Distributed under the [MIT License](LICENSE).
