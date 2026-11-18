#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/../../tools/env.sh"
out="$PORT_ROOT/out/hello"
mkdir -p "$out"
$ATMO_CC $ATMO_CFLAGS -O2 -Wall -Wextra -c "$here/hello.c" -o "$out/hello.o"
"$ATMO_LD" $ATMO_LDFLAGS -o "$out/hello.elf" "$out/hello.o" "$ATMO_RT_LIB"
echo "$out/hello.elf"
