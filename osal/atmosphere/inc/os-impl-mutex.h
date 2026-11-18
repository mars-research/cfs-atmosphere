#ifndef OS_IMPL_MUTEX_H
#define OS_IMPL_MUTEX_H

#include "osconfig.h"
#include "atmo_rt.h"

typedef struct
{
    art_mutex_t mutex;
} OS_impl_mutex_internal_record_t;

extern OS_impl_mutex_internal_record_t OS_impl_mutex_table[OS_MAX_MUTEXES];

#endif /* OS_IMPL_MUTEX_H */
