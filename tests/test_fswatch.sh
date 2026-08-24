#!/usr/bin/env bash
set -euo pipefail

bin_dir="${1:?usage: test_fswatch.sh <bin-dir>}"
watch_dir="$(mktemp -d /tmp/toyfs-fswatch.XXXXXX)"
log_file="/tmp/toyfs-fswatch.log"
rm -f "$log_file"

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

run_status_case "fswatch-usage" 0 "$bin_dir/fswatch"
run_status_case "fswatch-missing-dir" 2 "$bin_dir/fswatch" "$watch_dir/missing" "$log_file"

FSWATCH_MAX_EVENTS=5 "$bin_dir/fswatch" "$watch_dir" "$log_file" &
pid="$!"

cleanup() {
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
}
trap cleanup EXIT

sleep 0.2
printf alpha >"$watch_dir/watched.txt"
chmod 600 "$watch_dir/watched.txt"
mv "$watch_dir/watched.txt" "$watch_dir/moved.txt"
rm -f "$watch_dir/moved.txt"

for _ in {1..50}; do
    if ! kill -0 "$pid" 2>/dev/null; then
        break
    fi
    sleep 0.1
done

if kill -0 "$pid" 2>/dev/null; then
    echo "fswatch: watcher did not exit after expected events" >&2
    cleanup
    exit 1
fi

wait "$pid"
trap - EXIT

grep -Eq "watched.txt|moved.txt" "$log_file"
grep -Eq "OPEN|MODIFY|CLOSE_WRITE|ATTRIB|MOVED_FROM|MOVED_TO|DELETE|UNKNOWN" "$log_file"

stdout_dir="$(mktemp -d /tmp/toyfs-fswatch-stdout.XXXXXX)"
FSWATCH_MAX_EVENTS=1 "$bin_dir/fswatch" "$stdout_dir" >"/tmp/toyfs-fswatch-stdout.out" &
pid="$!"
trap cleanup EXIT
sleep 0.2
printf beta >"$stdout_dir/stdout.txt"
wait "$pid"
trap - EXIT

grep -q "stdout.txt" "/tmp/toyfs-fswatch-stdout.out"
