#!/usr/bin/env bash
# Verifies the Atmosphere kernel (atmosphere/kernel/verified) with Verus.
#
#   tools/verify-atmosphere.sh [verus options]
#
# This is what `atmo verify` does inside the Atmosphere nix shell. The first
# run builds Verus in .tools/verus: the revision of mars-research/verus that
# atmosphere/Cargo.lock pins, without std and alloc, as the Atmosphere flake
# builds it. That needs rustup (Verus pins its own Rust toolchain) and
# downloads Z3.
set -euo pipefail
. "$(dirname "$0")/env.sh"
unset RUSTC_BOOTSTRAP   # set by env.sh for the kernel build; Verus must not inherit it

verus_dir="$PORT_ROOT/.tools/verus"
verus="$verus_dir/source/target-verus/release/verus"
rev="$(sed -n 's|.*mars-research/verus?branch=mars#\([0-9a-f]*\)".*|\1|p' \
    "$ATMO_ROOT/Cargo.lock" | head -n 1)"

if [ ! -x "$verus" ]; then
    if [ ! -d "$verus_dir" ]; then
        git clone https://github.com/mars-research/verus "$verus_dir"
        git -C "$verus_dir" checkout --detach "$rev"
    fi
    (
        cd "$verus_dir/source"
        [ -x z3 ] || ./tools/get-z3.sh
        . ../tools/activate
        vargo build --release --vstd-no-std --vstd-no-alloc
    )
fi

built="$(git -C "$verus_dir" rev-parse HEAD)"
if [ "$built" != "$rev" ]; then
    echo "warning: .tools/verus is at $built, Cargo.lock pins $rev" >&2
fi

# Verus writes nothing we keep; run it outside the source tree.
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cd "$work"
"$verus" --crate-type lib "$ATMO_ROOT/kernel/verified/lib.rs" "$@"
