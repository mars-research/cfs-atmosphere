#!/usr/bin/env bash
# Boots the flight software on Atmosphere under QEMU and checks the first
# milestone of the port on the console output:
#
#   - the cFE reaches its OPERATIONAL state with the applications started;
#   - a no-op command raises the command counter of SAMPLE_APP in telemetry;
#   - the executive restarts SAMPLE_APP on command.
#
# The image must have been built with tools/build-cfs.sh. The log is left in
# out/cfs/run.log.
set -uo pipefail
. "$(dirname "$0")/env.sh"
log="$PORT_ROOT/out/cfs/run.log"

"$PORT_ROOT/tools/run-qemu.sh" "$PORT_ROOT/out/cfs/cfs-cpu1.img" "${1:-120}" >"$log" 2>&1
status=$?

failed=0
check() { # description, command...
    local what="$1"
    shift
    if "$@" >/dev/null 2>&1; then
        echo "ok    $what"
    else
        echo "FAIL  $what"
        failed=1
    fi
}

check "the run ended by itself with status 0 (got $status)" test "$status" = 0
check "cFE reached the OPERATIONAL state" grep -q "CFE_ES_Main entering OPERATIONAL state" "$log"
for app in "SAMPLE Lib Initialized" "SCH Lab Initialized" "Sample App Initialized" "HK Initialized" \
    "TO_TEST: started" "CI_TEST: started"; do
    check "started: $app" grep -q "$app" "$log"
done

# The command counter of SAMPLE_APP as telemetry shows it over the run:
# two no-ops, a restart, one more no-op.
counters="$(grep "TO_TEST: SAMPLE_APP_HK" "$log" | sed 's/.*cmd=\([0-9]*\).*/\1/' | uniq | tr '\n' ' ')"
check "SAMPLE_APP command counter in telemetry went 0 1 2 0 1 (got: $counters)" test "$counters" = "0 1 2 0 1 "
check "the executive restarted SAMPLE_APP" grep -q "Restart Application SAMPLE_APP Success" "$log"

packets="$(grep -c "TO_TEST: SAMPLE_APP_HK" "$log")"
check "SAMPLE_APP housekeeping arrived about once a second ($packets packets)" test "$packets" -ge 15

if [ "$failed" = 0 ]; then
    echo "cFS on Atmosphere: all checks passed"
else
    echo "cFS on Atmosphere: FAILED, see $log"
fi
exit "$failed"
