# upback

`upback` restores files from a backup created by `backup`.

## Usage

```bash
upback [--to DIR] BACKUP_NAME
```

`BACKUP_NAME` may be a name under `~/Backups` or a backup directory path.

## Options

- `--to DIR` - restore into `DIR` instead of the original source path recorded
  in `.source_path`.

## Behavior

- Restores regular files from the backup tree.
- Skips `.versions` and `versions.tar.gz`.
- Recreates destination directories as needed.
- Prompts before overwriting an existing destination file.

If `--to` is not provided, `upback` reads:

```text
<backup>/.source_path
```

and restores into that path.

## Exit Status

- `0` - command completed.
- `1` - invalid arguments, missing `$HOME`, missing backup, missing source path, or restore failure.
