# diskusage

`diskusage` scans a directory tree with the shared crawler and prints a compact
disk usage report.

## Usage

```bash
diskusage <path/to/dir>
```

## Output

The report includes:

- total number of regular files;
- top 10 largest files;
- total size of `.log` files;
- total size of `.tmp` files;
- total size of hidden files;
- total size of all regular files.

## Behavior

- The input path must exist and be a directory.
- Symlinks are not followed by the crawler configuration used here.
- Regular files are processed concurrently by the crawler callback.

## Exit Status

- `0` - success.
- `1` - invalid arguments or inaccessible/non-directory input.
- `2` - allocation or synchronization setup error.
- `3` - crawler failure.
