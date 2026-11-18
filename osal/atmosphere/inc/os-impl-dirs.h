#ifndef OS_IMPL_DIRS_H
#define OS_IMPL_DIRS_H

#include "osconfig.h"
#include "common_types.h"
#include "osapi-constants.h"

typedef struct
{
    char   path[OS_MAX_LOCAL_PATH_LEN];
    uint32 cursor;
} OS_impl_dir_internal_record_t;

extern OS_impl_dir_internal_record_t OS_impl_dir_table[OS_MAX_NUM_OPEN_DIRS];

#endif /* OS_IMPL_DIRS_H */
