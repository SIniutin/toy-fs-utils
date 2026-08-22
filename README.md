# Toy-fs-utils

A collection of small utilities for Linux/Unix systems, written in pure C.
The main focus of the project is a high-performance concurrent directory crawler.
The current branch focuses on stabilizing the queue/crawler contracts and the
utilities built on top of them.

## Motivation

File system traversal looks trivial until it needs to be fast, scalable, and safe.
This project was created to explore different concurrency patterns for directory
crawling, understand their trade-offs, and measure real performance differences
under various workloads.

The utilities in this repository serve both as practical tools and as experiments
in concurrent programming and systems-level design.

## Utilities

### diskusage

Concurrent disk usage analyzer with an emphasis on directory traversal performance.

### trashbin

Safe file removal utility that moves files to a trash directory instead of deleting
them permanently.

Features:
- Restore (untrash) files and directories
- List trashbin contents and statistics

### fswatch

File system watcher for tracking changes in directories and files.

### backup

Simple backup utility for copying and synchronizing directories.

Features:
- Multiple backup snapshots
- Listing and managing existing backups

## Directory Crawler

The directory crawler is the core component of the project.
It is designed to traverse large directory trees efficiently while keeping resource
usage predictable.

The crawler is reused across utilities such as `diskusage`, `trashbin`, and
`backup` through a shared callback interface.

Key aspects:
- Configurable worker pool size (defined at compile time via constants)
- Bounded ring-buffer task queue
- Graceful cancellation and error propagation
- Minimal synchronization overhead

## Documentation

User-facing utility documentation lives in [`docs/`](docs/README.md).

## Benchmarks

Benchmarks are planned after the core contracts are covered by tests.
Planned measurements include:
- Total traversal time
- CPU utilization
- Memory consumption
- Scalability with respect to the number of workers

## Tests

Unit tests and concurrency stress tests are planned next. The immediate target is
to cover the queue, crawler, and a trash end-to-end flow.

```bash
ctest
