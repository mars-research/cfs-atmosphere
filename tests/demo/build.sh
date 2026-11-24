#!/usr/bin/env bash
# Builds out/demo/demo_test.img: the demo without cFS. A plain C program
# plays the mission-critical process; the bus and the payload are the real
# ones, in a tar archive behind the image.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
. "$here/../../tools/env.sh"
out="$PORT_ROOT/out/demo"
demo="$PORT_ROOT/demo"
"$PORT_ROOT/tools/build-demo.sh" >/dev/null
cflags="$ATMO_CFLAGS -O2 -Wall -Wextra -I$demo/include -I$demo/loader -I$demo/mc"
for src in "$demo/loader/elf_load.c" "$demo/mc/demo_mc.c" "$here/demo_test.c"; do
    $ATMO_CC $cflags -c "$src" -o "$out/$(basename "${src%.c}").o"
done
"$ATMO_LD" $ATMO_LDFLAGS -o "$out/demo_test.elf" "$out/demo_test.o" "$out/demo_mc.o" "$out/elf_load.o" \
    "$PORT_ROOT/out/libc/libatmo_c.a" "$ATMO_RT_LIB"
rm -rf "$out/tar" && mkdir -p "$out/tar/cf"
cp "$out/bus.elf" "$out/payload.elf" "$out/tar/cf/"
COPYFILE_DISABLE=1 tar --format=ustar -cf "$out/cf.tar" -C "$out/tar" cf
cat "$out/demo_test.elf" "$out/cf.tar" >"$out/demo_test.img"
echo "$out/demo_test.img"
