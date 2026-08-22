# fswatch

`fswatch` watches a directory with Linux `inotify` and prints filesystem events.

## Usage

```bash
fswatch <dir-name> [log-name]
```

If `log-name` is provided, events are written to that file. Otherwise events are
written to stdout.

## Output

Each event is printed as:

```text
[YYYY-MM-DD HH:MM], <path>, <event>
```

Known event names include `ACCESS`, `ATTRIB`, `CREATE`, `DELETE`, `MODIFY`,
`MOVED_FROM`, `MOVED_TO`, `OPEN`, `CLOSE_WRITE`, and `CLOSE_NOWRITE`.

## Notes

- This utility runs until interrupted.
- It currently watches one directory path.
- It does not recursively add watches for subdirectories.

## Exit Status

- `0` - usage message or normal return.
- `1` - failed to initialize inotify.
- `2` - failed to add/read the watch.
