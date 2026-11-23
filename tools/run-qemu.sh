#!/usr/bin/env bash
# Boots the kernel under QEMU with the given ELF image as the first process.
#
#   tools/run-qemu.sh <image.elf> [timeout-seconds]
#
# Prints what the image wrote to the console, and the kernel's warnings and
# errors. Exits with the exit code that the image passed to art_exit(), 124 on
# a timeout, or 125 if QEMU stopped without the image ending the run (for
# example after a kernel panic).
#
# Set ATMO_QEMU_ACCEL=kvm on a Linux host with /dev/kvm; the kernel must have
# been built with the same setting. Set ATMO_QEMU_RAW=1 to print the whole
# serial log, including the loader and the kernel. ATMO_QEMU_SMP sets the
# number of processors (default 1; the kernel starts the others offline).
set -uo pipefail
. "$(dirname "$0")/env.sh"

image="${1:?usage: run-qemu.sh <image.elf> [timeout-seconds]}"
limit="${2:-120}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# The loader takes the kernel and the first process concatenated in one module.
cat "$ATMO_KERNEL" "$image" >"$work/initrd"

if [ "$ATMO_QEMU_ACCEL" = kvm ]; then
    accel=(-enable-kvm -cpu host,+invtsc -machine q35,kernel_irqchip=split)
else
    accel=(-cpu max -machine q35)
fi

# The kernel manages the first 4 GiB and needs an Intel IOMMU.
qemu-system-x86_64 "${accel[@]}" -smp "${ATMO_QEMU_SMP:-1}" -m 2304 \
    -nographic -serial mon:stdio -no-reboot -nic none \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    -device intel-iommu,aw-bits=48,intremap=on \
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
    # The kernel logs each line of a process as "[user@<cpu>] <line>".
    clean | awk '
        match($0, /\[user@[0-9]+\] ?/) { print substr($0, RSTART + RLENGTH); next }
        /entering first thread/ { started = 1 }
        started && /^ *(WARN|ERROR) / { print }'
fi
[ -n "${ATMO_QEMU_LOG:-}" ] && clean >"$ATMO_QEMU_LOG"

if [ "$timed_out" = 1 ]; then
    echo "run-qemu: timeout after ${limit}s" >&2
    exit 124
fi
# The kernel also leaves through the exit device when it panics.
if [ $((status & 1)) = 1 ] && ! clean | grep -q "panic!"; then
    exit $((status >> 1))
fi
echo "run-qemu: QEMU stopped with status $status before the image exited" >&2
[ -z "${ATMO_QEMU_RAW:-}" ] && clean | tail -n 25 >&2
exit 125
