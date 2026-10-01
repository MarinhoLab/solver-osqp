#!/usr/bin/env bash
# Runs every built benchmark variant and captures its RESULT lines.
# Usage: ./bench/run.sh [variant ...]   (default: all variants in bench/bin)
set -euo pipefail
BENCH="$(cd "$(dirname "$0")" && pwd)"
BIN="$BENCH/bin"
OUT="$BENCH/results"
mkdir -p "$OUT"

variants=("$@")
if [ "${#variants[@]}" -eq 0 ]; then
    variants=(o0 o1 o2 o3 ofast o3_native o3_native_lto ofast_native_lto)
fi

for v in "${variants[@]}"; do
    exe="$BIN/$v"
    if [ ! -x "$exe" ]; then
        echo "missing $exe, building..." >&2
        "$BENCH/build.sh" "$v" >/dev/null
    fi
    echo "=== running $v ===" >&2
    "$exe" | tee "$OUT/$v.txt"
done
echo "All done. Results in $OUT/" >&2
