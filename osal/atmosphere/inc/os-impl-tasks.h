#ifndef OS_IMPL_TASKS_H
#define OS_IMPL_TASKS_H

#include "osconfig.h"
#include "atmo_rt.h"

typedef struct
{
    art_task_t handle;
} OS_impl_task_internal_record_t;

extern OS_impl_task_internal_record_t OS_impl_task_table[OS_MAX_TASKS];

#endif /* OS_IMPL_TASKS_H */
