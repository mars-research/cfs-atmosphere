# Source this file. It sets the paths that the other scripts use.
#
# Tools needed: rustup (it reads atmosphere/rust-toolchain.toml), clang with
# the x86-64 backend, nasm, GNU ld and objcopy for x86-64 ELF, and
# qemu-system-x86_64. `nix develop` puts them all on PATH.

PORT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Atmosphere and cFS are submodules of this repository. ATMO_ROOT and CFS_ROOT
# point the scripts at other checkouts.
ATMO_ROOT="${ATMO_ROOT:-$PORT_ROOT/atmosphere}"
CFS_ROOT="${CFS_ROOT:-$PORT_ROOT/cFS}"
if [ ! -e "$ATMO_ROOT/Cargo.toml" ] || [ ! -e "$CFS_ROOT/cfe" ]; then
    echo "missing submodules: run git submodule update --init --recursive" >&2
    return 1 2>/dev/null || exit 1
fi
ATMO_ROOT="$(cd "$ATMO_ROOT" && pwd)"
[ -d "$CFS_ROOT" ] && CFS_ROOT="$(cd "$CFS_ROOT" && pwd)"

# rust-lld and the LLVM binutils ship with the Rust toolchain.
RUST_SYSROOT="$(cd "$ATMO_ROOT" && rustc --print sysroot)"
RUST_HOST="$(cd "$ATMO_ROOT" && rustc -vV | sed -n 's/^host: //p')"
RUST_LLVM_BIN="$RUST_SYSROOT/lib/rustlib/$RUST_HOST/bin"

ATMO_TARGET_DIR="$ATMO_ROOT/target"
ATMO_LOADER="$ATMO_TARGET_DIR/x86_64-unknown-none/release/aloader"
ATMO_KERNEL="$ATMO_TARGET_DIR/x86_64-unknown-aloader/release/kernel"
ATMO_RT_LIB="$ATMO_TARGET_DIR/x86_64-unknown-aloader/release/libatmo_rt.a"
ATMO_RT_INC="$ATMO_ROOT/user/atmo-rt/include"

# Flags for C code that runs in an Atmosphere process. The kernel does not
# save vector or floating-point state, so the code uses general-purpose
# registers only.
ATMO_CC="${ATMO_CC:-clang}"
ATMO_CFLAGS="--target=x86_64-unknown-none-elf -ffreestanding -fPIE -mno-red-zone \
-mgeneral-regs-only -fno-stack-protector -U_FORTIFY_SOURCE -nostdlibinc \
-fno-builtin-printf -I$ATMO_RT_INC -I$PORT_ROOT/libc/include"
ATMO_LD="$RUST_LLVM_BIN/rust-lld"
ATMO_LDFLAGS="-flavor gnu -pie --no-dynamic-linker --entry=_start -z noexecstack --gc-sections"
ATMO_AR="$RUST_LLVM_BIN/llvm-ar"

export PATH RUSTC_BOOTSTRAP=1
