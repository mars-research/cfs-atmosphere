#!/usr/bin/env bash
# Builds the loader, the kernel, and the C runtime library.
set -euo pipefail
. "$(dirname "$0")/env.sh"
rustup target list --installed | grep -qx "$ATMO_RUST_TARGET" || rustup target add "$ATMO_RUST_TARGET"
[ -x "$ATMO_AR" ] || rustup component add llvm-tools

run() { # directory, command...
    local dir="$1" log
    shift
    log="$(mktemp)"
    if ! (cd "$dir" && "$@") >"$log" 2>&1; then
        tail -n 40 "$log"
        rm -f "$log"
        echo "build in $dir failed" >&2
        exit 2
    fi
    rm -f "$log"
}

features=()
[ -n "$ATMO_KERNEL_FEATURES" ] && features=(--features "$ATMO_KERNEL_FEATURES")
run "$ATMO_ROOT/boot" cargo build --release -p aloader -p kernel ${features[@]+"${features[@]}"}
run "$PORT_ROOT/rt" cargo build --release --target "$ATMO_RUST_TARGET" --target-dir "$PORT_ROOT/out/rt"
ls -l "$ATMO_LOADER" "$ATMO_KERNEL" "$ATMO_RT_LIB"
