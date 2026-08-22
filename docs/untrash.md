# untrash

`untrash` restores files recorded in `~/.trash.log`.

## Usage

```bash
untrash [--overwrite|--unique] [--to DIR] PATTERN
```

`PATTERN` is matched against the original file basename with `fnmatch`.

## Options

- `--to DIR` - restore matching files into `DIR`.
- `--overwrite` - overwrite an existing destination file.
- `--unique` - choose a unique destination name such as `name(1).ext`.

`--overwrite` and `--unique` are mutually exclusive.

## Behavior

For each matching trash entry, `untrash` asks for confirmation:

```text
Restore? [y/n]
```

If the original directory still exists, the file is restored there. If it does
not exist and `--to` was not provided, the file is restored under:

```text
~/restore-lost
```

## Storage

Payload files are read from:

```text
~/.trash/<trash_link_name>
```

Metadata is read from:

```text
~/.trash.log
```

## Exit Status

- `0` - command completed, or the trash log/directory does not exist.
- `1` - invalid arguments, missing `$HOME`, mutually exclusive options, or destination conflict without `--overwrite`/`--unique`.
- `2` - filesystem or I/O error while reading trash metadata or restoring a file.
- `3` - internal error.
