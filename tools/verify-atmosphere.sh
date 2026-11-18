#!/usr/bin/env bash
# Verifies the Atmosphere kernel (atmosphere/kernel/verified) with Verus.
#
#   tools/verify-atmosphere.sh [verus options]
#
# This is what `atmo verify` does inside the Atmosphere nix shell. Verus comes
# from this repository's flake (`nix build .#verus`); the first run builds it,
# which takes a while. Set VERUS to use another rust_verify.
set -euo pipefail
. "$(dirname "$0")/env.sh"
unset RUSTC_BOOTSTRAP   # set by env.sh for the kernel build; Verus must not inherit it

verus="${VERUS:-}"
if [ -z "$verus" ]; then
    # The kernel must be verified with the Verus whose vstd it is built with.
    rev="$(sed -n 's|.*mars-research/verus?branch=mars#\([0-9a-f]*\)".*|\1|p' \
        "$ATMO_ROOT/Cargo.lock" | head -n 1)"
    if ! grep -q "\"rev\": \"$rev\"" "$PORT_ROOT/flake.lock"; then
        echo "warning: flake.lock does not pin Verus $rev, which atmosphere/Cargo.lock uses;" >&2
        echo "         run: nix flake update verus" >&2
    fi
    verus="$(nix build --no-link --print-out-paths "$PORT_ROOT#verus")/bin/rust_verify"
fi

# Verus writes nothing we keep; run it outside the source tree.
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cd "$work"
"$verus" --crate-type lib "$ATMO_ROOT/kernel/verified/lib.rs" "$@"
