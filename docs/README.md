# Toy FS Utilities Documentation

This directory documents the user-facing utilities in the repository.

## Design

- [`design`](design.md) - queue and crawler invariants.

## Utilities

- [`diskusage`](diskusage.md) - concurrent disk usage report for a directory tree.
- [`fswatch`](fswatch.md) - inotify-based filesystem event watcher.
- [`rm_trash`](rm_trash.md) - safe removal by moving files into `~/.trash`.
- [`list_trash`](list_trash.md) - list entries recorded in `~/.trash.log`.
- [`untrash`](untrash.md) - restore entries from trash.
- [`backup`](backup.md) - create/update backups under `~/Backups`.
- [`backup_list`](backup_list.md) - list available backups.
- [`upback`](upback.md) - restore a backup.

## Build

```bash
./build.sh
```

Built binaries are written to `build/bin/`.

## Tests

```bash
ctest --test-dir build --output-on-failure
```

For an isolated Linux run:

```bash
docker build -f tests/Dockerfile -t toyfs-tests .
docker run --rm toyfs-tests
```
