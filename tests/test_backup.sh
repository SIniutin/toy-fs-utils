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
run_status_case "backup-extra-arg" 1 env HOME="$tmp_home" "$bin_dir/backup" "$src" "$src"
run_status_case "backup-compress-check" 1 env HOME="$tmp_home" "$bin_dir/backup" --compress --check "$src"
run_status_case "backup-non-directory" 1 env HOME="$tmp_home" "$bin_dir/backup" "$src/missing"
not_dir="$(mktemp /tmp/toyfs-backup-not-dir.XXXXXX)"
run_status_case "backup-existing-file-source" 1 env HOME="$tmp_home" "$bin_dir/backup" "$not_dir"
run_status_case "backup-home-unset" 1 env -u HOME "$bin_dir/backup" "$src"
run_status_case "backup-list-empty" 0 env HOME="$tmp_home" "$bin_dir/backup_list"
run_status_case "backup-list-bad-n" 1 env HOME="$tmp_home" "$bin_dir/backup_list" -n
run_status_case "backup-list-unknown-option" 1 env HOME="$tmp_home" "$bin_dir/backup_list" --bad
run_status_case "backup-list-home-unset" 1 env -u HOME "$bin_dir/backup_list"
empty_home="$(mktemp -d /tmp/toyfs-empty-home.XXXXXX)"
mkdir -p "$empty_home/Backups"
run_status_case "backup-list-no-backups" 0 env HOME="$empty_home" "$bin_dir/backup_list"
run_status_case "upback-missing" 1 env HOME="$tmp_home" "$bin_dir/upback" missing
run_status_case "upback-missing-path-version" 1 env HOME="$tmp_home" "$bin_dir/upback" "$tmp_home/Backups/missing"
run_status_case "upback-usage" 1 env HOME="$tmp_home" "$bin_dir/upback"
run_status_case "upback-to-missing-arg" 1 env HOME="$tmp_home" "$bin_dir/upback" --to
run_status_case "upback-extra-arg" 1 env HOME="$tmp_home" "$bin_dir/upback" one two
run_status_case "upback-home-unset" 1 env -u HOME "$bin_dir/upback" missing

mkdir -p "$src/sub"
printf alpha >"$src/a.txt"
printf beta >"$src/sub/b.txt"

long_home="/tmp/$(printf 'x%.0s' {1..5000})"
run_status_case "backup-long-home" 1 env HOME="$long_home" "$bin_dir/backup" "$src"
run_status_case "backup-list-long-home" 1 env HOME="$long_home" "$bin_dir/backup_list"
run_status_case "upback-long-home" 1 env HOME="$long_home" "$bin_dir/upback" missing

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

restore_path="$(mktemp -d /tmp/toyfs-backup-restore-path.XXXXXX)"
HOME="$tmp_home" "$bin_dir/upback" --to "$restore_path" "$backup_dir" >"/tmp/toyfs-upback-path.out"
grep -q "Restoring from:" "/tmp/toyfs-upback-path.out"
test "$(cat "$restore_path/a.txt")" = "alpha"
test "$(cat "$restore_path/sub/b.txt")" = "beta"

restore_new="/tmp/toyfs-upback-new-$RANDOM"
HOME="$tmp_home" "$bin_dir/upback" --to "$restore_new" "$backup_name" >"/tmp/toyfs-upback-new-dest.out"
test "$(cat "$restore_new/a.txt")" = "alpha"
test "$(cat "$restore_new/sub/b.txt")" = "beta"

restore_file="$(mktemp /tmp/toyfs-upback-file.XXXXXX)"
run_status_case "upback-dest-is-file" 0 env HOME="$tmp_home" "$bin_dir/upback" --to "$restore_file" "$backup_name"
run_status_case "upback-bad-to-parent" 1 env HOME="$tmp_home" "$bin_dir/upback" --to "/dev/null/child" "$backup_name"

printf 'n\nn\n' | HOME="$tmp_home" "$bin_dir/upback" "$backup_name" >"/tmp/toyfs-upback-skip.out"
grep -q "Skipped:" "/tmp/toyfs-upback-skip.out"

printf 'y\ny\n' | HOME="$tmp_home" "$bin_dir/upback" "$backup_name" >"/tmp/toyfs-upback-overwrite.out"
grep -q "Target exists:" "/tmp/toyfs-upback-overwrite.out"
test "$(cat "$src/a.txt")" = "alpha"
test "$(cat "$src/sub/b.txt")" = "beta"

printf changed >"$src/a.txt"
printf new >"$src/new.txt"

HOME="$tmp_home" "$bin_dir/backup" --check "$src" >"/tmp/toyfs-backup-check-changed.out"
grep -q "Changed: a.txt" "/tmp/toyfs-backup-check-changed.out"
grep -q "New: new.txt" "/tmp/toyfs-backup-check-changed.out"
grep -q "sub/b.txt" "/tmp/toyfs-backup-check-changed.out"

HOME="$tmp_home" "$bin_dir/backup" "$src" >"/tmp/toyfs-backup-update.out"
grep -q "New: new.txt" "/tmp/toyfs-backup-update.out"
grep -q "Updated: a.txt" "/tmp/toyfs-backup-update.out"
grep -q "sub/b.txt" "/tmp/toyfs-backup-update.out"
test "$(cat "$backup_dir/a.txt")" = "changed"
test "$(cat "$backup_dir/new.txt")" = "new"

HOME="$tmp_home" "$bin_dir/backup_list" -s -n 1 >"/tmp/toyfs-backup-list-size.out"
grep -q "$backup_name" "/tmp/toyfs-backup-list-size.out"

HOME="$tmp_home" "$bin_dir/backup_list" -t >"/tmp/toyfs-backup-list-time.out"
grep -q "$backup_name" "/tmp/toyfs-backup-list-time.out"

printf compressed >"$src/a.txt"
HOME="$tmp_home" "$bin_dir/backup" --compress "$src" >"/tmp/toyfs-backup-compress.out"
test -f "$backup_dir/versions.tar.gz"

src_restore_original="$(mktemp -d /tmp/toyfs-backup-original.XXXXXX)"
printf original >"$src_restore_original/original.txt"
HOME="$tmp_home" "$bin_dir/backup" "$src_restore_original" >"/tmp/toyfs-backup-original.out"
backup_original_dir="$(find "$tmp_home/Backups" -mindepth 1 -maxdepth 1 -type d -name "$(basename "$src_restore_original")-*" | head -n 1)"
backup_original_name="$(basename "$backup_original_dir")"
rm -rf "$src_restore_original"
HOME="$tmp_home" "$bin_dir/upback" "$backup_original_name" >"/tmp/toyfs-upback-original.out"
test "$(cat "$src_restore_original/original.txt")" = "original"

manual_backup="$tmp_home/Backups/manual-2026-08-24"
mkdir -p "$manual_backup"
printf manual >"$manual_backup/file.txt"
run_status_case "upback-no-source-path" 1 env HOME="$tmp_home" "$bin_dir/upback" "$(basename "$manual_backup")"
