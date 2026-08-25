#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 ROOT wide|deep|mixed FILE_COUNT" >&2
}

root="${1:-}"
shape="${2:-}"
count="${3:-}"

if [ -z "$root" ] || [ -z "$shape" ] || [ -z "$count" ]; then
    usage
    exit 1
fi

case "$count" in
    ''|*[!0-9]*)
        usage
        exit 1
        ;;
esac

if [ "$count" -eq 0 ]; then
    echo "FILE_COUNT must be greater than zero" >&2
    exit 1
fi

mkdir -p "$root"
if find "$root" -mindepth 1 -maxdepth 1 | read -r _; then
    echo "Refusing to write into non-empty directory: $root" >&2
    exit 1
fi

write_file() {
    local path="$1"
    printf 'toyfs-bench:%s\n' "$path" >"$path"
}

make_wide() {
    local dirs=32
    local i
    for ((i = 0; i < dirs; ++i)); do
        mkdir -p "$root/dir-$i"
    done

    for ((i = 0; i < count; ++i)); do
        write_file "$root/dir-$((i % dirs))/file-$i.txt"
    done
}

make_deep() {
    local max_depth=128
    local path="$root"
    local i
    for ((i = 0; i < max_depth; ++i)); do
        path="$path/d-$i"
        mkdir -p "$path"
    done

    for ((i = 0; i < count; ++i)); do
        write_file "$path/file-$i.txt"
        if ((i % 256 == 255)); then
            path="$root/d-$((i / 256))"
            mkdir -p "$path"
        fi
    done
}

make_mixed() {
    local top=16
    local mid=8
    local i
    for ((i = 0; i < count; ++i)); do
        local dir="$root/t-$((i % top))/m-$(((i / top) % mid))"
        mkdir -p "$dir"
        write_file "$dir/file-$i.txt"
    done
}

case "$shape" in
    wide)
        make_wide
        ;;
    deep)
        make_deep
        ;;
    mixed)
        make_mixed
        ;;
    *)
        usage
        exit 1
        ;;
esac

echo "Created $shape fixture with $count files at $root"
