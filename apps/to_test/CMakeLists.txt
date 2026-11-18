project(CFS_TO_TEST C)

# Telemetry output for tests: prints telemetry packets on the console.
add_cfe_app(to_test fsw/src/to_test.c)
add_cfe_app_dependency(to_test sample_app hk)
