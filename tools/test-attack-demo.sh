#!/usr/bin/env bash
# Boots the flight software with the attack demo under QEMU with four
# processors, waits for the script of CI_TEST to run every scenario, and
# checks the console log.
#
#   tools/test-attack-demo.sh [timeout-seconds]
#
# The image must have been built with tools/build-cfs.sh. The log is left in
# out/cfs/attack-demo.log; tools/attack-demo-report.py checks it and prints
# the pass/fail table, the evidence lines, and the measurements.
set -uo pipefail
. "$(dirname "$0")/env.sh"
log="$PORT_ROOT/out/cfs/attack-demo.log"

ATMO_QEMU_SMP=4 "$PORT_ROOT/tools/run-qemu.sh" "$PORT_ROOT/out/cfs/cfs-cpu1.img" "${1:-240}" >"$log" 2>&1
status=$?
python3 "$PORT_ROOT/tools/attack-demo-report.py" "$log"
report=$?
echo "the run ended with status $status; log: $log"
[ "$status" = 0 ] && [ "$report" = 0 ]
