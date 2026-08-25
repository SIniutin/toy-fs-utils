#!/usr/bin/env bash
set -euo pipefail

bin_dir="${1:?usage: test_diskusage.sh <bin-dir>}"
work="$(mktemp -d /tmp/toyfs-diskusage.XXXXXX)"

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

run_status_case "diskusage-missing" 1 "$bin_dir/diskusage" "$work/missing"
run_status_case "diskusage-usage" 1 "$bin_dir/diskusage"

mkdir -p "$work/sub"
printf alpha >"$work/a.txt"
printf beta >"$work/sub/b.log"
printf gamma >"$work/sub/c.tmp"
printf hide >"$work/.hidden"

run_status_case "diskusage-file-input" 1 "$bin_dir/diskusage" "$work/a.txt"

"$bin_dir/diskusage" "$work" >"/tmp/toyfs-diskusage.out"

grep -q "Files: 4" "/tmp/toyfs-diskusage.out"
grep -q "Top-10:" "/tmp/toyfs-diskusage.out"
grep -q "a.txt (5 B)" "/tmp/toyfs-diskusage.out"
grep -q "b.log (4 B)" "/tmp/toyfs-diskusage.out"
grep -q "c.tmp (5 B)" "/tmp/toyfs-diskusage.out"
grep -q ".hidden (4 B)" "/tmp/toyfs-diskusage.out"
grep -q "Logs: 4 B" "/tmp/toyfs-diskusage.out"
grep -q "Temp: 5 B" "/tmp/toyfs-diskusage.out"
grep -q "Hidden: 4 B" "/tmp/toyfs-diskusage.out"
grep -q "Total: 18 B" "/tmp/toyfs-diskusage.out"

wide="$(mktemp -d /tmp/toyfs-diskusage-wide.XXXXXX)"
for i in {1..12}; do
    printf '%*s' "$i" '' | tr ' ' x >"$wide/file_$i.bin"
done

"$bin_dir/diskusage" "$wide" >"/tmp/toyfs-diskusage-wide.out"
grep -q "Files: 12" "/tmp/toyfs-diskusage-wide.out"
grep -q "file_12.bin (12 B)" "/tmp/toyfs-diskusage-wide.out"
grep -q "Total: 78 B" "/tmp/toyfs-diskusage-wide.out"
