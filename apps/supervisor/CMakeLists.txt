project(CFS_SUPERVISOR C)

# Loads the bus and the payload into child containers, watches them, and
# restarts them. The logic is in demo/mc, which does not depend on cFE.
set(DEMO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../demo)
add_cfe_app(supervisor
    fsw/src/supervisor.c
    ${DEMO_DIR}/mc/demo_mc.c
    ${DEMO_DIR}/loader/elf_load.c
)
target_include_directories(supervisor PUBLIC ${DEMO_DIR}/include ${DEMO_DIR}/mc ${DEMO_DIR}/loader)
