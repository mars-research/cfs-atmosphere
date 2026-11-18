# OSAL configuration for Atmosphere.

# Atmosphere has no network stack and no dynamic loader.
set(OSAL_CONFIG_INCLUDE_NETWORK        FALSE)
set(OSAL_CONFIG_INCLUDE_DYNAMIC_LOADER FALSE)
set(OSAL_CONFIG_INCLUDE_STATIC_LOADER  TRUE)

# OS_printf() writes in the calling task.
set(OSAL_CONFIG_CONSOLE_ASYNC FALSE)

set(OSAL_CONFIG_DEBUG_PRINTF FALSE)
set(OSAL_CONFIG_MAX_MUTEXES  40)
