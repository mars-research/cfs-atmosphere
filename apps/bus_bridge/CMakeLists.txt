project(CFS_BUS_BRIDGE C)

# Carries messages between the cFE software bus and the demo's bus container.
set(DEMO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../demo)
add_cfe_app(bus_bridge fsw/src/bus_bridge.c)
target_include_directories(bus_bridge PUBLIC ${DEMO_DIR}/include ${DEMO_DIR}/mc)
add_cfe_app_dependency(bus_bridge supervisor)
