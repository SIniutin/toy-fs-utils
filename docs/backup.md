# backup

`backup` creates or updates a backup of a directory under `~/Backups`.

## Usage

```bash
backup [--compress] [--check] DIR
```

## Options

- `--check` - compare source files with the backup and print what would be new,
  changed, or unchanged without copying files.
- `--compress` - archive the `.versions` directory into `versions.tar.gz`.

`--compress` and `--check` cannot be used together.

## Backup Layout

Backups are written to:

```text
~/Backups/<source-basename>-<YYYY-MM-DD>
```

The backup directory contains:

- copied source files with relative paths preserved;
- `.source_path` with the original source directory;
- `.versions/` containing older versions of updated files.

When a file already exists in the backup and has changed, the old backup copy is
moved into `.versions` and the new copy replaces it.

## Exit Status

- `0` - command completed.
- `1` - invalid arguments or source/backup setup error.
- `2` - allocation or synchronization setup error.
