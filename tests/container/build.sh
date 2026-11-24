#!/usr/bin/env bash
# Builds out/container/container_test.img: the test, followed by the image
# of the child that it loads into a container.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/../../tools/env.sh"
out="$PORT_ROOT/out/container"
demo="$PORT_ROOT/demo"
mkdir -p "$out"
"$PORT_ROOT/libc/build.sh" >/dev/null
cflags="$ATMO_CFLAGS -O2 -Wall -Wextra -I$demo/include -I$demo/guest -I$demo/loader"

$ATMO_CC $cflags -c "$demo/guest/guest.c" -o "$out/guest.o"
$ATMO_CC $cflags -fno-builtin -c "$demo/guest/mem.c" -o "$out/mem.o"
$ATMO_CC $cflags -c "$here/child.c" -o "$out/child.o"
"$ATMO_LD" $ATMO_LDFLAGS -o "$out/child.elf" "$out/child.o" "$out/guest.o" "$out/mem.o" \
    "$PORT_ROOT/out/libc/libatmo_c.a"

$ATMO_CC $cflags -c "$demo/loader/elf_load.c" -o "$out/elf_load.o"
$ATMO_CC $cflags -c "$here/container_test.c" -o "$out/container_test.o"
"$ATMO_LD" $ATMO_LDFLAGS -o "$out/container_test.elf" "$out/container_test.o" "$out/elf_load.o" \
    "$PORT_ROOT/out/libc/libatmo_c.a" "$ATMO_RT_LIB"
cat "$out/container_test.elf" "$out/child.elf" >"$out/container_test.img"
echo "$out/container_test.img"
