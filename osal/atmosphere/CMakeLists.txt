######################################################################
#
# CMake build recipe for the Atmosphere OSAL implementation
#
# The directory is outside the OSAL tree. The OSAL build finds it through
# OSAL_EXT_SOURCE_DIR when OSAL_SYSTEM_OSTYPE is "atmosphere".
#
######################################################################

set(ATMOSPHERE_BASE_SRCLIST
    src/os-impl-binsem.c
    src/os-impl-clock.c
    src/os-impl-common.c
    src/os-impl-console.c
    src/os-impl-countsem.c
    src/os-impl-dirs.c
    src/os-impl-files.c
    src/os-impl-filesys.c
    src/os-impl-heap.c
    src/os-impl-idmap.c
    src/os-impl-mutex.c
    src/os-impl-queues.c
    src/os-impl-tasks.c
    src/os-impl-timebase.c
)

# Parts of the OSAL that are shared between systems, and stubs for the
# features that Atmosphere does not have.
set(OSAL_PORTABLE_DIR "${OSAL_SOURCE_DIR}/src/os/portable")
set(ATMOSPHERE_IMPL_SRCLIST
    ${OSAL_PORTABLE_DIR}/os-impl-console-bsp.c
    ${OSAL_PORTABLE_DIR}/os-impl-no-condvar.c
    ${OSAL_PORTABLE_DIR}/os-impl-no-rwlock.c
    ${OSAL_PORTABLE_DIR}/os-impl-no-file-allocate.c
    ${OSAL_PORTABLE_DIR}/os-impl-no-shell.c
    ${OSAL_PORTABLE_DIR}/os-impl-no-loader.c
    ${OSAL_PORTABLE_DIR}/os-impl-no-symtab.c
    ${OSAL_PORTABLE_DIR}/os-impl-no-network-gethostid.c
    ${OSAL_PORTABLE_DIR}/os-impl-no-network-gethostname.c
    ${OSAL_PORTABLE_DIR}/os-impl-no-sockets.c
    ${OSAL_PORTABLE_DIR}/os-impl-no-select.c
)

if (OSAL_CONFIG_INCLUDE_DYNAMIC_LOADER)
    message(FATAL_ERROR "Atmosphere has no dynamic loader: set OSAL_CONFIG_INCLUDE_DYNAMIC_LOADER to FALSE")
endif ()
if (OSAL_CONFIG_INCLUDE_NETWORK)
    message(FATAL_ERROR "Atmosphere has no network stack yet: set OSAL_CONFIG_INCLUDE_NETWORK to FALSE")
endif ()

add_library(osal_atmosphere_impl OBJECT
    ${ATMOSPHERE_BASE_SRCLIST}
    ${ATMOSPHERE_IMPL_SRCLIST}
)

target_include_directories(osal_atmosphere_impl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_compile_definitions(osal_public_api INTERFACE
    _ATMOSPHERE_OS_
)
