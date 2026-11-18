# Builds the OSAL for Atmosphere together with the OSAL's own functional
# tests (osal/src/tests). Each test is one Atmosphere image.
#
# The OSAL's ENABLE_UNIT_TESTS switch is not used, because it also builds the
# line-coverage tests, which need a hosted C library.
cmake_minimum_required(VERSION 3.10)
project(OSAL_ATMOSPHERE_TESTS C)

set(OSAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../cFS/osal" CACHE PATH "OSAL source tree")

set(OSAL_CONFIG_INCLUDE_NETWORK        FALSE CACHE BOOL "" FORCE)
set(OSAL_CONFIG_INCLUDE_DYNAMIC_LOADER FALSE CACHE BOOL "" FORCE)
set(OSAL_CONFIG_INCLUDE_STATIC_LOADER  TRUE  CACHE BOOL "" FORCE)
set(OSAL_CONFIG_CONSOLE_ASYNC          FALSE CACHE BOOL "" FORCE)
set(OSAL_CONFIG_DEBUG_PRINTF           FALSE CACHE BOOL "" FORCE)

# Only what the tests need is built from the OSAL tree.
add_subdirectory("${OSAL_DIR}" osal EXCLUDE_FROM_ALL)

# The test library without utstubs.c, which converts floating-point values.
set(UT_ASSERT_DIR "${OSAL_DIR}/ut_assert")
add_library(ut_assert_atmosphere STATIC
    ${UT_ASSERT_DIR}/src/utassert.c
    ${UT_ASSERT_DIR}/src/utlist.c
    ${UT_ASSERT_DIR}/src/uttest.c
    ${UT_ASSERT_DIR}/src/uttools.c
    ${UT_ASSERT_DIR}/src/utbsp.c
)
target_include_directories(ut_assert_atmosphere PRIVATE ${OSAL_DIR}/src/bsp/shared/inc)
target_include_directories(ut_assert_atmosphere PUBLIC ${UT_ASSERT_DIR}/inc)
target_compile_definitions(ut_assert_atmosphere PUBLIC "_UNIT_TEST_")
target_link_libraries(ut_assert_atmosphere PUBLIC osal_bsp osal)

file(GLOB OSAL_TESTS "${OSAL_DIR}/src/tests/*-test")
foreach(OSTEST ${OSAL_TESTS})
    get_filename_component(TESTNAME ${OSTEST} NAME)
    # The port has no network stack; the test expects one.
    if (TESTNAME STREQUAL "network-api-test")
        continue()
    endif ()
    set(TESTFILES)
    aux_source_directory(${OSTEST} TESTFILES)
    add_executable(${TESTNAME} ${TESTFILES})
    target_link_libraries(${TESTNAME} PUBLIC ut_assert_atmosphere osal)
    # The toolchain file puts these two libraries on every link line.
    set_property(TARGET ${TESTNAME} APPEND PROPERTY LINK_DEPENDS "${ATMO_RT_LIB}" "${ATMO_C_LIB}")
endforeach()
