#!/usr/bin/env bash
# Builds out/libc/libatmo_c.a.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/../tools/env.sh"
out="$PORT_ROOT/out/libc"
mkdir -p "$out"
objs=()
for src in "$here"/src/*.c; do
    obj="$out/$(basename "${src%.c}").o"
    # One section per function: a program that uses part of the library does
    # not need what the rest refers to.
    $ATMO_CC $ATMO_CFLAGS -O2 -Wall -Wextra -ffunction-sections -fdata-sections -c "$src" -o "$obj"
    objs+=("$obj")
done
rm -f "$out/libatmo_c.a"
"$ATMO_AR" rcs "$out/libatmo_c.a" "${objs[@]}"
echo "$out/libatmo_c.a"
