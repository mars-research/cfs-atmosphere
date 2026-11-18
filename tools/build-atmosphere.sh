#!/usr/bin/env bash
# Builds the Atmosphere loader, the kernel, and the C runtime library.
set -euo pipefail
. "$(dirname "$0")/env.sh"
for crate in aloader kernel user/atmo-rt; do
    log="$(mktemp)"
    if ! (cd "$ATMO_ROOT/$crate" && cargo build --release) >"$log" 2>&1; then
        tail -n 40 "$log"
        echo "build of $crate failed" >&2
        exit 2
    fi
    rm -f "$log"
done
ls -l "$ATMO_LOADER" "$ATMO_KERNEL" "$ATMO_RT_LIB"
