#ifndef OS_IMPL_QUEUES_H
#define OS_IMPL_QUEUES_H

#include "osconfig.h"
#include "atmo_rt.h"

typedef struct
{
    art_queue_t queue;
} OS_impl_queue_internal_record_t;

extern OS_impl_queue_internal_record_t OS_impl_queue_table[OS_MAX_QUEUES];

#endif /* OS_IMPL_QUEUES_H */
