######################################################################
#
# CMake build recipe for the Atmosphere BSP on a PC (x86-64)
#
######################################################################

add_library(osal_pc-atmosphere_impl OBJECT
    src/bsp_start.c
    src/bsp_console.c
)

# This BSP only works with the "atmosphere" OS layer.
set_property(TARGET osal_pc-atmosphere_impl PROPERTY OSAL_EXPECTED_OSTYPE "atmosphere")
