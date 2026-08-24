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
run_status_case "list bad -n" 1 env HOME="$tmp_home" "$bin_dir/list_trash" -n nope
run_status_case "rm_trash rejects directory without -r" 1 env HOME="$tmp_home" "$bin_dir/rm_trash" "$work"

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

HOME="$tmp_home" "$bin_dir/list_trash" --grep file.txt >/tmp/toyfs-list.out
grep -q "file.txt" /tmp/toyfs-list.out

printf 'y\n' | HOME="$tmp_home" "$bin_dir/untrash" 'file.txt' >/tmp/toyfs-untrash.out
test -e "$work/file.txt"
grep -q "Restored to:" /tmp/toyfs-untrash.out

printf replaced > "$work/dir/a.log"
printf 'y\n' | HOME="$tmp_home" "$bin_dir/untrash" --unique 'a.log' >/tmp/toyfs-untrash-unique.out
test -e "$work/dir/a(1).log"
grep -q "Restored to:" /tmp/toyfs-untrash-unique.out
