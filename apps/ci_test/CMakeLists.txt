project(CFS_CI_TEST C)

# Command ingest for tests: sends commands from a built-in script.
add_cfe_app(ci_test fsw/src/ci_test.c)
add_cfe_app_dependency(ci_test sample_app)
target_include_directories(ci_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../demo/include)
