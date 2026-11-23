# CMake toolchain for C programs that run in an Atmosphere process on x86-64.
#
# The compiler is clang for a freestanding ELF target. The linker and the
# archiver are the LLVM tools that ship with the Rust toolchain. Every
# executable is linked with the port's C library and the Atmosphere C runtime.
#
# Locations can be overridden with the environment variables ATMO_ROOT (the
# kernel tree) and ATMO_CC (the compiler).

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_SYSTEM_VERSION   1)

get_filename_component(ATMO_PORT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if (DEFINED ENV{ATMO_ROOT})
    set(ATMO_ROOT "$ENV{ATMO_ROOT}")
else ()
    get_filename_component(ATMO_ROOT "${ATMO_PORT_ROOT}/atmosphere-smp" ABSOLUTE)
endif ()

# rustc resolves the toolchain that the kernel tree pins.
execute_process(COMMAND rustc --print sysroot
    WORKING_DIRECTORY "${ATMO_ROOT}/boot"
    OUTPUT_VARIABLE ATMO_RUST_SYSROOT OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND rustc -vV
    WORKING_DIRECTORY "${ATMO_ROOT}/boot"
    OUTPUT_VARIABLE ATMO_RUSTC_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REGEX MATCH "host: ([^\n]+)" _unused "${ATMO_RUSTC_VERSION}")
set(ATMO_LLVM_BIN "${ATMO_RUST_SYSROOT}/lib/rustlib/${CMAKE_MATCH_1}/bin")

set(ATMO_RT_LIB "${ATMO_PORT_ROOT}/out/rt/x86_64-unknown-none/release/libatmo_rt.a")
set(ATMO_C_LIB  "${ATMO_PORT_ROOT}/out/libc/libatmo_c.a")

if (DEFINED ENV{ATMO_CC})
    set(CMAKE_C_COMPILER "$ENV{ATMO_CC}")
else ()
    set(CMAKE_C_COMPILER clang)
endif ()
set(CMAKE_C_COMPILER_TARGET x86_64-unknown-none-elf)
set(CMAKE_AR     "${ATMO_LLVM_BIN}/llvm-ar"    CACHE FILEPATH "archiver")
set(CMAKE_RANLIB "${ATMO_LLVM_BIN}/llvm-ar" CACHE FILEPATH "ranlib")
set(CMAKE_LINKER "${ATMO_LLVM_BIN}/rust-lld"   CACHE FILEPATH "linker")
set(CMAKE_RANLIB_FLAGS "s")
set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_AR> s <TARGET>")

# The compiler cannot link a test program by itself.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# The kernel gives user code no vector or floating-point state, so the code
# uses general-purpose registers only.
set(ATMO_C_FLAGS "-ffreestanding -fPIE -mno-red-zone -mgeneral-regs-only -fno-stack-protector -U_FORTIFY_SOURCE -nostdlibinc -fno-builtin-printf -ffunction-sections -fdata-sections -isystem ${ATMO_PORT_ROOT}/libc/include -isystem ${ATMO_PORT_ROOT}/rt/include")
set(CMAKE_C_FLAGS_INIT "${ATMO_C_FLAGS}")

set(CMAKE_EXECUTABLE_SUFFIX_C ".elf")
set(CMAKE_C_LINK_EXECUTABLE
    "<CMAKE_LINKER> -flavor gnu -pie --no-dynamic-linker --entry=_start -z noexecstack --gc-sections <LINK_FLAGS> <OBJECTS> -o <TARGET> --start-group <LINK_LIBRARIES> ${ATMO_C_LIB} ${ATMO_RT_LIB} --end-group")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Selections for the OSAL and the cFE build.
set(OSAL_SYSTEM_OSTYPE  "atmosphere")
set(OSAL_SYSTEM_BSPTYPE "pc-atmosphere")
set(CFE_SYSTEM_PSPNAME  "pc-atmosphere")
set(OSAL_EXT_SOURCE_DIR "${ATMO_PORT_ROOT}/osal" CACHE PATH "External OSAL OS and BSP layers")
