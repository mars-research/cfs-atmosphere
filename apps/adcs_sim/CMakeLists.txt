project(CFS_ADCS_SIM C)

# A stand-in for attitude control: it accepts pointing commands and reports
# the last one in its housekeeping telemetry.
set(DEMO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../demo)
add_cfe_app(adcs_sim fsw/src/adcs_sim.c)
target_include_directories(adcs_sim PUBLIC ${DEMO_DIR}/include)
