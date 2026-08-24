#!/usr/bin/env bash
set -euo pipefail

bin_dir="${1:?usage: test_backup.sh <bin-dir>}"
tmp_home="$(mktemp -d /tmp/toyfs-home.XXXXXX)"
src="$(mktemp -d /tmp/toyfs-backup-src.XXXXXX)"
restore="$(mktemp -d /tmp/toyfs-backup-restore.XXXXXX)"

run_status_case() {
    local name="$1"
    local expected="$2"
    shift 2

    set +e
    "$@" >"/tmp/toyfs-${name}.out" 2>"/tmp/toyfs-${name}.err"
    local status=$?
    set -e

    if [ "$status" -ne "$expected" ]; then
        echo "$name: expected exit $expected, got $status" >&2
        cat "/tmp/toyfs-${name}.out" >&2
        cat "/tmp/toyfs-${name}.err" >&2
        exit 1
    fi
}

run_status_case "backup-missing-arg" 1 env HOME="$tmp_home" "$bin_dir/backup"
run_status_case "backup-list-empty" 0 env HOME="$tmp_home" "$bin_dir/backup_list"
run_status_case "upback-missing" 1 env HOME="$tmp_home" "$bin_dir/upback" missing

mkdir -p "$src/sub"
printf alpha >"$src/a.txt"
printf beta >"$src/sub/b.txt"

HOME="$tmp_home" "$bin_dir/backup" "$src" >"/tmp/toyfs-backup.out"
grep -q "Summary: added 2, updated 0, skipped 0" "/tmp/toyfs-backup.out"

backup_count="$(find "$tmp_home/Backups" -mindepth 1 -maxdepth 1 -type d | wc -l)"
test "$backup_count" -eq 1

backup_dir="$(find "$tmp_home/Backups" -mindepth 1 -maxdepth 1 -type d)"
backup_name="$(basename "$backup_dir")"

test -f "$backup_dir/a.txt"
test -f "$backup_dir/sub/b.txt"
test -f "$backup_dir/.source_path"
grep -q "SOURCE=$src" "$backup_dir/.source_path"

HOME="$tmp_home" "$bin_dir/backup" --check "$src" >"/tmp/toyfs-backup-check.out"
grep -q "Unchanged:" "/tmp/toyfs-backup-check.out"
grep -q "a.txt" "/tmp/toyfs-backup-check.out"
grep -q "sub/b.txt" "/tmp/toyfs-backup-check.out"

HOME="$tmp_home" "$bin_dir/backup_list" >"/tmp/toyfs-backup-list.out"
grep -q "Available backups:" "/tmp/toyfs-backup-list.out"
grep -q "$backup_name" "/tmp/toyfs-backup-list.out"

HOME="$tmp_home" "$bin_dir/upback" --to "$restore" "$backup_name" >"/tmp/toyfs-upback.out"
grep -q "Restoring from:" "/tmp/toyfs-upback.out"
grep -q "Restored: $restore/a.txt" "/tmp/toyfs-upback.out"
grep -q "Restored: $restore/sub/b.txt" "/tmp/toyfs-upback.out"

test "$(cat "$restore/a.txt")" = "alpha"
test "$(cat "$restore/sub/b.txt")" = "beta"
