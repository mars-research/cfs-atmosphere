#ifndef OS_IMPL_TIMEBASE_H
#define OS_IMPL_TIMEBASE_H

#include <stdbool.h>
#include "osconfig.h"
#include "common_types.h"
#include "atmo_rt.h"

typedef struct
{
    art_mutex_t handler_mutex;
    art_task_t  handler_task;
    /* Given when the schedule changes, so that the helper task recomputes
     * its wait. */
    art_sem_t   changed;
    /* The layer provides the tick; the caller gave no sync function. */
    bool        simulate;
    /* The next tick is the first one after a set. */
    bool        reset;
    /* The clock value of the next tick; ART_FOREVER while not running. */
    uint64_t    next_tick;
    uint32      start_usec;
    uint32      interval_usec;
} OS_impl_timebase_internal_record_t;

extern OS_impl_timebase_internal_record_t OS_impl_timebase_table[OS_MAX_TIMEBASES];

#endif /* OS_IMPL_TIMEBASE_H */
