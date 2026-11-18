#!/usr/bin/env bash
# Boots Atmosphere under QEMU with the given ELF image as the first process.
#
#   tools/run-qemu.sh <image.elf> [timeout-seconds]
#
# Prints the serial output of the image. Exits with the exit code that the
# image passed to art_exit(), 124 on a timeout, or 125 if QEMU stopped without
# the image ending the run (for example after a kernel fault).
#
# Set ATMO_QEMU_ACCEL=kvm on a Linux host with /dev/kvm. Set ATMO_QEMU_RAW=1 to
# print the whole serial log, including the loader and the kernel.
set -uo pipefail
. "$(dirname "$0")/env.sh"

image="${1:?usage: run-qemu.sh <image.elf> [timeout-seconds]}"
limit="${2:-120}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# The loader takes the kernel and the first process concatenated in one module.
cat "$ATMO_KERNEL" "$image" >"$work/initrd"

accel=(-cpu max)
[ "${ATMO_QEMU_ACCEL:-tcg}" = kvm ] && accel=(-enable-kvm -cpu host)

# The loader needs more than 2 GiB of contiguous memory.
qemu-system-x86_64 "${accel[@]}" -machine q35,kernel_irqchip=split -smp 1 -m 6144 \
    -nographic -serial mon:stdio -no-reboot \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    -kernel "$ATMO_LOADER" -initrd "$work/initrd" </dev/null >"$work/serial.log" 2>&1 &
pid=$!
# The watchdog must not hold our output open, or a caller that captures it
# waits for the sleep to end.
( sleep "$limit"; kill "$pid" 2>/dev/null ) >/dev/null 2>&1 &
watchdog=$!
wait "$pid"
status=$?
if kill "$watchdog" 2>/dev/null; then wait "$watchdog" 2>/dev/null; timed_out=0; else timed_out=1; fi

clean() { sed -e $'s/\x1b\\[[0-9;]*m//g' -e 's/\r$//' "$work/serial.log"; }
if [ -n "${ATMO_QEMU_RAW:-}" ]; then
    clean
else
    # Everything after the kernel enters the first process.
    clean | awk 'show { sub(/^ *INFO /, ""); print } /Going to sysret into/ { show = 1 }'
fi
[ -n "${ATMO_QEMU_LOG:-}" ] && clean >"$ATMO_QEMU_LOG"

if [ "$timed_out" = 1 ]; then
    echo "run-qemu: timeout after ${limit}s" >&2
    exit 124
fi
if [ $((status & 1)) = 1 ]; then
    exit $((status >> 1))
fi
echo "run-qemu: QEMU stopped with status $status before the image exited" >&2
[ -z "${ATMO_QEMU_RAW:-}" ] && clean | tail -n 25 >&2
exit 125
