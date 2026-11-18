#!/usr/bin/env bash
# Runs the OSAL test images under QEMU, several at a time, and prints one
# line per test. Logs go to out/osal-tests/logs/.
#
#   tools/run-osal-tests.sh [test-name ...]
set -uo pipefail
. "$(dirname "$0")/env.sh"
dir="$PORT_ROOT/out/osal-tests"
logs="$dir/logs"
jobs="${ATMO_TEST_JOBS:-6}"
limit="${ATMO_TEST_TIMEOUT:-240}"
mkdir -p "$logs"

if [ $# -gt 0 ]; then
    tests=("$@")
else
    tests=()
    for elf in "$dir"/*.elf; do tests+=("$(basename "${elf%.elf}")"); done
fi

run_one() {
    local name="$1"
    "$PORT_ROOT/tools/run-qemu.sh" "$dir/$name.elf" "$limit" >"$logs/$name.log" 2>&1
    echo $? >"$logs/$name.status"
}

running=0
for name in "${tests[@]}"; do
    run_one "$name" &
    running=$((running + 1))
    if [ "$running" -ge "$jobs" ]; then
        wait -n 2>/dev/null || wait
        running=$((running - 1))
    fi
done
wait

failed=0
printf '%-36s %-8s %6s %6s %6s\n' TEST RESULT PASS FAIL OTHER
for name in "${tests[@]}"; do
    status="$(cat "$logs/$name.status")"
    # Sum the per-segment totals that the test framework prints.
    read -r pass fail other <<<"$(awk '/^\[  END\].*TOTAL::/ {
            for (i = 1; i <= NF; i++) {
                split($i, kv, "::");
                if (kv[1] == "PASS") p += kv[2];
                else if (kv[1] == "FAIL") f += kv[2];
                else if (kv[1] == "MIR" || kv[1] == "TSF" || kv[1] == "TTF") o += kv[2];
            }
        } END { printf "%d %d %d\n", p, f, o }' "$logs/$name.log")"
    case "$status" in
        0) result=ok ;;
        124) result=TIMEOUT ;;
        125) result=CRASH ;;
        *) result="FAIL($status)" ;;
    esac
    [ "$status" = 0 ] || failed=$((failed + 1))
    printf '%-36s %-8s %6s %6s %6s\n' "$name" "$result" "$pass" "$fail" "$other"
done
echo "$((${#tests[@]} - failed)) of ${#tests[@]} test programs passed"
[ "$failed" = 0 ]
