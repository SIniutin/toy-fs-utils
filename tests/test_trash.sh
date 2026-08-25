#!/usr/bin/env bash
set -euo pipefail

bin_dir="${1:?usage: test_trash.sh <bin-dir>}"

tmp_home="$(mktemp -d /tmp/toyfs-home.XXXXXX)"
work="$(mktemp -d /tmp/toyfs-trash.XXXXXX)"

run_status_case() {
    name="$1"
    expected="$2"
    shift 2

    set +e
    "$@" >/tmp/toyfs-case.out 2>/tmp/toyfs-case.err
    status="$?"
    set -e

    if [ "$status" -ne "$expected" ]; then
        echo "$name: expected status $expected, got $status" >&2
        cat /tmp/toyfs-case.out >&2
        cat /tmp/toyfs-case.err >&2
        exit 1
    fi
}

run_status_case "list empty trash" 0 env HOME="$tmp_home" "$bin_dir/list_trash"
run_status_case "list unknown option" 1 env HOME="$tmp_home" "$bin_dir/list_trash" --bad
run_status_case "list grep missing arg" 1 env HOME="$tmp_home" "$bin_dir/list_trash" --grep
run_status_case "list bad -n" 1 env HOME="$tmp_home" "$bin_dir/list_trash" -n nope
run_status_case "rm_trash no args" 0 env HOME="$tmp_home" "$bin_dir/rm_trash"
run_status_case "rm_trash help" 0 env HOME="$tmp_home" "$bin_dir/rm_trash" -h
run_status_case "rm_trash unknown option" 1 env HOME="$tmp_home" "$bin_dir/rm_trash" --bad
run_status_case "rm_trash nothing after option" 0 env HOME="$tmp_home" "$bin_dir/rm_trash" -v
run_status_case "rm_trash rejects directory without -r" 1 env HOME="$tmp_home" "$bin_dir/rm_trash" "$work"
run_status_case "rm_trash missing file" 2 env HOME="$tmp_home" "$bin_dir/rm_trash" "$work/missing.txt"
run_status_case "rm_trash home unset" 1 env -u HOME "$bin_dir/rm_trash" "$work/missing.txt"
run_status_case "untrash missing pattern" 1 env HOME="$tmp_home" "$bin_dir/untrash"
run_status_case "untrash bad mode combo" 1 env HOME="$tmp_home" "$bin_dir/untrash" --overwrite --unique anything
run_status_case "untrash to missing arg" 1 env HOME="$tmp_home" "$bin_dir/untrash" --to

long_home="/tmp/$(printf 'x%.0s' {1..5000})"
run_status_case "rm_trash rejects long HOME" 1 env HOME="$long_home" "$bin_dir/rm_trash" "$work/missing.txt"
run_status_case "list_trash rejects long HOME" 2 env HOME="$long_home" "$bin_dir/list_trash"
run_status_case "untrash rejects long HOME" 1 env HOME="$long_home" "$bin_dir/untrash" anything

bad_home="$(mktemp -d /tmp/toyfs-bad-home.XXXXXX)"
printf notdir > "$bad_home/.trash"
run_status_case "rm_trash rejects trash file" 1 env HOME="$bad_home" "$bin_dir/rm_trash" "$work/missing.txt"

mkdir -p "$work/dir/sub"
printf alpha > "$work/file.txt"
printf beta > "$work/dir/a.log"
printf gamma > "$work/dir/sub/b.tmp"

HOME="$tmp_home" "$bin_dir/rm_trash" -r "$work/file.txt" "$work/dir"

test ! -e "$work/file.txt"
test ! -e "$work/dir/a.log"
test ! -e "$work/dir/sub/b.tmp"
test -d "$tmp_home/.trash"
test -f "$tmp_home/.trash.log"

printf verbose > "$work/verbose.txt"
HOME="$tmp_home" "$bin_dir/rm_trash" -v "$work/verbose.txt" >/tmp/toyfs-rm-verbose.out
grep -q "rm_trash: link" /tmp/toyfs-rm-verbose.out
grep -q "rm_trash: unlink" /tmp/toyfs-rm-verbose.out

HOME="$tmp_home" "$bin_dir/list_trash" --grep file.txt >/tmp/toyfs-list.out
grep -q "file.txt" /tmp/toyfs-list.out

printf 'y\n' | HOME="$tmp_home" "$bin_dir/untrash" 'file.txt' >/tmp/toyfs-untrash.out
test -e "$work/file.txt"
grep -q "Restored to:" /tmp/toyfs-untrash.out

printf replaced > "$work/dir/a.log"
printf 'y\n' | HOME="$tmp_home" "$bin_dir/untrash" --unique 'a.log' >/tmp/toyfs-untrash-unique.out
test -e "$work/dir/a(1).log"
grep -q "Restored to:" /tmp/toyfs-untrash-unique.out

printf delta > "$work/skip.txt"
printf 'n\n' | HOME="$tmp_home" "$bin_dir/rm_trash" -i "$work/skip.txt" >/tmp/toyfs-rm-skip.out
test -e "$work/skip.txt"

HOME="$tmp_home" "$bin_dir/rm_trash" "$work/skip.txt"
printf conflict > "$work/skip.txt"
run_status_case "untrash rejects conflict" 1 bash -c "printf 'y\n' | HOME='$tmp_home' '$bin_dir/untrash' 'skip.txt'"
test "$(cat "$work/skip.txt")" = "conflict"

printf 'y\n' | HOME="$tmp_home" "$bin_dir/untrash" --overwrite 'skip.txt' >/tmp/toyfs-untrash-overwrite.out
test "$(cat "$work/skip.txt")" = "delta"
grep -q "Restored to:" /tmp/toyfs-untrash-overwrite.out

printf target > "$work/to-dir.txt"
HOME="$tmp_home" "$bin_dir/rm_trash" "$work/to-dir.txt"
custom_restore="$(mktemp -d /tmp/toyfs-untrash-to.XXXXXX)"
printf 'y\n' | HOME="$tmp_home" "$bin_dir/untrash" --to "$custom_restore" 'to-dir.txt' >/tmp/toyfs-untrash-to.out
test "$(cat "$custom_restore/to-dir.txt")" = "target"
grep -q "Restored to:" /tmp/toyfs-untrash-to.out

printf missing > "$work/missing-source.txt"
HOME="$tmp_home" "$bin_dir/rm_trash" "$work/missing-source.txt"
trash_link="$(awk -F ' \\| ' '/missing-source.txt/ { print $2; exit }' "$tmp_home/.trash.log")"
rm -f "$tmp_home/.trash/$trash_link"
run_status_case "untrash missing trash source" 2 bash -c "printf 'y\n' | HOME='$tmp_home' '$bin_dir/untrash' 'missing-source.txt'"

if [ -d /dev/shm ] && [ -w /dev/shm ]; then
    printf cross-device > "$work/cross-device.txt"
    HOME="$tmp_home" "$bin_dir/rm_trash" "$work/cross-device.txt"
    cross_restore="$(mktemp -d /dev/shm/toyfs-untrash.XXXXXX)"
    printf 'y\n' | HOME="$tmp_home" "$bin_dir/untrash" --to "$cross_restore" 'cross-device.txt' >/tmp/toyfs-untrash-cross.out
    test "$(cat "$cross_restore/cross-device.txt")" = "cross-device"
    grep -q "Restored to:" /tmp/toyfs-untrash-cross.out
fi
