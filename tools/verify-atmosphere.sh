#!/usr/bin/env bash
# Verifies the kernel (atmosphere-smp/src) with Verus.
#
#   tools/verify-atmosphere.sh [verus options]
#
# The kernel is verified with the Verus of its own submodule. The first run
# builds it (atmosphere-smp/verus/source/target-verus), which downloads Z3. The run
# itself is the kernel's verify.sh; it uses flock, which the nix shell has.
set -euo pipefail
. "$(dirname "$0")/env.sh"

# Verus pins its own toolchain; do not hand it the one of the kernel build.
unset RUSTUP_TOOLCHAIN
if [ ! -x "$ATMO_ROOT/verus/source/target-verus/release/verus" ]; then
    (
        cd "$ATMO_ROOT/verus/source"
        # shellcheck disable=SC1091
        . ../tools/activate
        ./tools/get-z3.sh
        vargo build --release
    )
fi
cd "$ATMO_ROOT"
status=0
./verify.sh --num-threads "${ATMO_VERIFY_THREADS:-8}" "$@" || status=$?
# verify.sh numbers its runs in a tracked file. A run from here is not a
# change to the kernel; leave the tree as it was.
git checkout -- .verus-log/verify-count
exit "$status"
