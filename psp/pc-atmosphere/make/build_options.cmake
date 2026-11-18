# Build options of the Atmosphere PSP.

# Files of the non-volatile volume are installed in this directory. The build
# packs it into the boot payload.
set(INSTALL_SUBDIR "cf")

add_definitions("-D_ATMOSPHERE_OS_")
