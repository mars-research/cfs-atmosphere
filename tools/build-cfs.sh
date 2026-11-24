#!/usr/bin/env bash
# Builds the flight software for Atmosphere.
#
# Result: out/cfs/cfs-cpu1.img, the cFE executable followed by a tar archive
# of the /cf volume (startup script and tables). tools/run-qemu.sh boots it.
set -euo pipefail
. "$(dirname "$0")/env.sh"

build="$PORT_ROOT/out/cfs"
"$PORT_ROOT/tools/build-atmosphere.sh" >/dev/null
"$PORT_ROOT/libc/build.sh" >/dev/null
# The bus and the payload of the attack demo, which SUPERVISOR loads from /cf.
"$PORT_ROOT/tools/build-demo.sh" >/dev/null

# The PSP build has no hook for a directory outside its tree.
ln -sfn "$PORT_ROOT/psp/pc-atmosphere" "$CFS_ROOT/psp/fsw/pc-atmosphere"

# The table build extracts objects from target archives with the host
# archiver. The archiver of macOS cannot read them, so the host build uses
# the LLVM one too.
# The test applications of the port are outside the cFS tree.
export CFS_APP_PATH="$PORT_ROOT/apps"

host_flags=()
if [ "$(uname -s)" = Darwin ]; then
    # One of the host tools includes <endian.h>, which macOS does not have.
    host_flags=(-DCMAKE_C_FLAGS="-I$PORT_ROOT/tools/host-compat/darwin")
fi
if [ ! -f "$build/CMakeCache.txt" ]; then
    cmake -S "$CFS_ROOT/cfe" -B "$build" \
        -DMISSIONCONFIG=atmo -DMISSION_DEFS="$PORT_ROOT/atmo_defs" \
        -DCMAKE_BUILD_TYPE=release -DCMAKE_INSTALL_PREFIX=/exe \
        -DENABLE_UNIT_TESTS=FALSE \
        -DCMAKE_AR="$ATMO_AR" "${host_flags[@]}"
fi
make -C "$build" mission-install DESTDIR="$build" -j"${ATMO_BUILD_JOBS:-8}"

exe="$build/exe/cpu1"

# The executive refuses to restart an application whose module file does not
# exist. Applications are linked into the image, so the file is never read;
# an empty one satisfies the check.
for line in $(sed -n 's/^CFE_APP, *\([a-z_0-9]*\),.*/\1/p' "$exe/cf/cfe_es_startup.scr"); do
    : >"$exe/cf/$line.so"
done
cp "$PORT_ROOT/out/demo/bus.elf" "$PORT_ROOT/out/demo/payload.elf" "$exe/cf/"
# macOS tar would add extended-attribute entries without this.
COPYFILE_DISABLE=1 tar --format=ustar -cf "$build/cf.tar" -C "$exe" cf
cat "$exe/core-cpu1.elf" "$build/cf.tar" >"$build/cfs-cpu1.img"
ls -l "$exe/core-cpu1.elf" "$exe/cf" "$build/cfs-cpu1.img"
