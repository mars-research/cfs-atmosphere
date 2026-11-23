# Source this file. It sets the paths that the other scripts use.
#
# Tools needed: rustup, clang with the x86-64 backend, nasm, GNU ld and
# objcopy for x86-64 ELF, and qemu-system-x86_64. `nix develop` puts them all
# on PATH.

PORT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# The kernel and cFS are submodules of this repository. The kernel is
# VeriFlat, the concurrent Atmosphere kernel, in the mars-research clone.
# ATMO_ROOT and CFS_ROOT point the scripts at other checkouts.
ATMO_ROOT="${ATMO_ROOT:-$PORT_ROOT/atmosphere-smp}"
CFS_ROOT="${CFS_ROOT:-$PORT_ROOT/cFS}"
if [ ! -e "$ATMO_ROOT/boot/Cargo.toml" ] || [ ! -e "$ATMO_ROOT/verus/source/vstd" ] || [ ! -e "$CFS_ROOT/cfe" ]; then
    echo "missing submodules: run tools/get-submodules.sh" >&2
    return 1 2>/dev/null || exit 1
fi
ATMO_ROOT="$(cd "$ATMO_ROOT" && pwd)"
CFS_ROOT="$(cd "$CFS_ROOT" && pwd)"

# The kernel pins the Rust toolchain; the runtime is built with the same one.
RUSTUP_TOOLCHAIN="$(sed -n 's/^channel *= *"\(.*\)"/\1/p' "$ATMO_ROOT/boot/rust-toolchain.toml")"
export RUSTUP_TOOLCHAIN
ATMO_RUST_TARGET=x86_64-unknown-none

# rust-lld and the LLVM binutils ship with the Rust toolchain.
RUST_SYSROOT="$(rustc --print sysroot)"
RUST_HOST="$(rustc -vV | sed -n 's/^host: //p')"
RUST_LLVM_BIN="$RUST_SYSROOT/lib/rustlib/$RUST_HOST/bin"

# QEMU without KVM emulates neither PCID nor INVPCID. The kernel is then built
# with the feature that does without them.
ATMO_QEMU_ACCEL="${ATMO_QEMU_ACCEL:-tcg}"
if [ "$ATMO_QEMU_ACCEL" = kvm ]; then
    ATMO_KERNEL_FEATURES="${ATMO_KERNEL_FEATURES-}"
else
    ATMO_KERNEL_FEATURES="${ATMO_KERNEL_FEATURES-kernel/no-pcid}"
fi

ATMO_TARGET_DIR="$ATMO_ROOT/boot/target"
ATMO_LOADER="$ATMO_TARGET_DIR/$ATMO_RUST_TARGET/release/aloader"
ATMO_KERNEL="$ATMO_TARGET_DIR/$ATMO_RUST_TARGET/release/kernel"
ATMO_RT_LIB="$PORT_ROOT/out/rt/$ATMO_RUST_TARGET/release/libatmo_rt.a"
ATMO_RT_INC="$PORT_ROOT/rt/include"

# Flags for C code that runs in a process. The kernel gives user code no
# vector or floating-point state, so the code uses general-purpose registers
# only.
ATMO_CC="${ATMO_CC:-clang}"
ATMO_CFLAGS="--target=x86_64-unknown-none-elf -ffreestanding -fPIE -mno-red-zone \
-mgeneral-regs-only -fno-stack-protector -U_FORTIFY_SOURCE -nostdlibinc \
-fno-builtin-printf -I$ATMO_RT_INC -I$PORT_ROOT/libc/include"
ATMO_LD="$RUST_LLVM_BIN/rust-lld"
ATMO_LDFLAGS="-flavor gnu -pie --no-dynamic-linker --entry=_start -z noexecstack --gc-sections"
ATMO_AR="$RUST_LLVM_BIN/llvm-ar"

export PATH
