#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/../../tools/env.sh"
out="$PORT_ROOT/out/rt"
mkdir -p "$out"
"$PORT_ROOT/libc/build.sh" >/dev/null
$ATMO_CC $ATMO_CFLAGS -O2 -Wall -Wextra -c "$here/rt_test.c" -o "$out/rt_test.o"
"$ATMO_LD" $ATMO_LDFLAGS -o "$out/rt_test.elf" "$out/rt_test.o" \
    "$PORT_ROOT/out/libc/libatmo_c.a" "$ATMO_RT_LIB"
echo "$out/rt_test.elf"
