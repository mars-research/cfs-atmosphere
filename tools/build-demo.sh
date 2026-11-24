#!/usr/bin/env bash
# Builds the images of the attack demo that run in child containers:
# out/demo/bus.elf and out/demo/payload.elf.
set -euo pipefail
. "$(dirname "$0")/env.sh"
out="$PORT_ROOT/out/demo"
demo="$PORT_ROOT/demo"
mkdir -p "$out"
"$PORT_ROOT/libc/build.sh" >/dev/null

log="$(mktemp)"
if ! (cd "$demo/bus" && cargo build --release --target "$ATMO_RUST_TARGET" --target-dir "$PORT_ROOT/out/bus") >"$log" 2>&1; then
    tail -n 40 "$log"
    rm -f "$log"
    echo "build of the bus failed" >&2
    exit 2
fi
rm -f "$log"
cp "$PORT_ROOT/out/bus/$ATMO_RUST_TARGET/release/bus" "$out/bus.elf"

cflags="$ATMO_CFLAGS -O2 -Wall -Wextra -I$demo/include -I$demo/guest"
$ATMO_CC $cflags -c "$demo/guest/guest.c" -o "$out/guest.o"
$ATMO_CC $cflags -fno-builtin -c "$demo/guest/mem.c" -o "$out/mem.o"
$ATMO_CC $cflags -c "$demo/payload/payload.c" -o "$out/payload.o"
"$ATMO_LD" $ATMO_LDFLAGS -o "$out/payload.elf" "$out/payload.o" "$out/guest.o" "$out/mem.o" \
    "$PORT_ROOT/out/libc/libatmo_c.a"
ls -l "$out/bus.elf" "$out/payload.elf"
