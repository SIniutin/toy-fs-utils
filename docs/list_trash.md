# list_trash

`list_trash` lists files recorded by `rm_trash` in `~/.trash.log`.

## Usage

```bash
list_trash [-s|-t] [-n N] [--grep PATTERN]
```

## Options

- `-s` - sort by size, descending.
- `-t` - sort by timestamp, descending.
- `-n N` - show at most `N` entries.
- `--grep PATTERN` - keep only entries whose basename contains `PATTERN`.

## Output

The table contains:

- original path;
- trash link name;
- inode;
- size;
- deletion timestamp.

## Storage

`list_trash` reads:

```text
~/.trash.log
```

The current log format is:

```text
original_path | trash_link_name | inode | size | timestamp
```

## Exit Status

- `0` - success.
- `1` - invalid option, invalid option argument, or missing `$HOME`.
- `2` - filesystem or I/O error while reading `~/.trash.log`.
- `3` - internal allocation error.

If `~/.trash.log` does not exist, the trash is treated as empty and the command
exits with status `0`.
