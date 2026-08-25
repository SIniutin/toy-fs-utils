# backup_list

`backup_list` lists backup directories stored under `~/Backups`.

## Usage

```bash
backup_list [-n N] [-s|-t]
```

## Options

- `-t` - sort by date, descending. This is the default.
- `-s` - sort by total size, descending.
- `-n N` - show at most `N` backups.

## Output

Each backup is printed as:

```text
<name> | <files> files, <versions> versions, <size>K total
```

The file count excludes `.versions`; version count is counted from `.versions`.

## Exit Status

- `0` - success, including "no backups found".
- `1` - invalid arguments, missing `$HOME`, or scan/allocation error.
