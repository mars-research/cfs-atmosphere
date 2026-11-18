#ifndef OS_IMPL_IO_H
#define OS_IMPL_IO_H

#include <stdbool.h>
#include "osconfig.h"
#include "common_types.h"

typedef struct
{
    /* A handle of the runtime's file system, or -1. */
    int fd;
} OS_impl_file_internal_record_t;

extern OS_impl_file_internal_record_t OS_impl_filehandle_table[OS_MAX_NUM_OPEN_FILES];

#endif /* OS_IMPL_IO_H */
