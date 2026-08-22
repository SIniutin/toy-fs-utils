# rm_trash

`rm_trash` moves regular files to a trash storage by creating a hard link in
`~/.trash` and unlinking the original path.

This is a same-filesystem trash implementation: the file data is preserved
through a hard link, not copied.

## Usage

```bash
rm_trash [-h] [-v] [-i] [-r] FILE...
```

## Restrictions

1. Fails with `EXDEV` if source and trash are on different filesystems.
2. Only regular files are moved to trash.
3. Directory arguments require `-r`.

## Options

- `-h` - print help and exit 0.
- `-v` - verbose mode.
- `-i` - prompt on stdin before processing each file.
- `-r` - recursively traverse directory arguments and process regular files inside them.

## Exit status

- `0` - success.
- `1` - user error, invalid arguments, or environment error.
- `2` - filesystem or I/O error.
- `3` - internal traversal error.

If source and trash are on different filesystems, rm_trash fails with EXDEV and exits with status 2.

## Trash layout

Files are stored in:

```text
~/.trash/<original-name>@<timestamp>@<inode>
```

Metadata is currently appended to:

```text
~/.trash.log
```

Each log entry has:

```text
original_path | trash_link_name | inode | size | timestamp
```

If `$HOME` environ is not set, `rm_trash` exits with an error code `1`

## Notes

The project plan keeps a future `.trash/.meta/<id>` layout as a possible cleanup,
but the current implementation uses `~/.trash.log` because `list_trash` and
`untrash` consume that format.
