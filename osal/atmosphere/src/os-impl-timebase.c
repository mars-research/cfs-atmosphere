/*
 * Time bases.
 *
 * Each time base has a helper task that runs OS_TimeBase_CallbackThread().
 * When the caller gives no sync function, the layer provides the tick: the
 * helper task waits until the clock value of the next tick. Ticks are placed
 * at start + n * interval, so they do not drift.
 */
#include "os-atmosphere.h"
#include "os-impl-timebase.h"
#include "os-shared-timebase.h"
#include "os-shared-idmap.h"
#include "os-shared-common.h"

#define OS_ATMOSPHERE_TIMEBASE_STACK_SIZE (64 * 1024)

OS_impl_timebase_internal_record_t OS_impl_timebase_table[OS_MAX_TIMEBASES];

void OS_TimeBaseLock_Impl(const OS_object_token_t *token)
{
    OS_impl_timebase_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);

    art_mutex_lock(&impl->handler_mutex);
}

void OS_TimeBaseUnlock_Impl(const OS_object_token_t *token)
{
    OS_impl_timebase_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);

    art_mutex_unlock(&impl->handler_mutex);
}

/* Waits for the next tick and returns the time since the last one in microseconds. */
static uint32 OS_Atmosphere_TimeBaseWait(osal_id_t timebase_id)
{
    OS_object_token_t                   token;
    OS_impl_timebase_internal_record_t *impl;
    uint64_t                            tick;
    uint32                              elapsed;

    if (OS_ObjectIdGetById(OS_LOCK_MODE_NONE, OS_OBJECT_TYPE_OS_TIMEBASE, timebase_id, &token) != OS_SUCCESS)
    {
        return 0;
    }
    impl = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, token);

    for (;;)
    {
        tick = impl->next_tick;
        switch (art_sem_take(&impl->changed, tick))
        {
            case ART_OK:
                /* OS_TimeBaseSet changed the schedule. */
                continue;
            case ART_TIMEOUT:
                break;
            default:
                return 0;
        }
        if (impl->reset)
        {
            elapsed     = impl->start_usec;
            impl->reset = false;
        }
        else
        {
            elapsed = impl->interval_usec;
        }
        impl->next_tick = (impl->interval_usec != 0) ? tick + (uint64_t)impl->interval_usec * 1000u : ART_FOREVER;
        return elapsed;
    }
}

int32 OS_Atmosphere_TimeBaseAPI_Impl_Init(void)
{
    memset(OS_impl_timebase_table, 0, sizeof(OS_impl_timebase_table));

    /* The clock has nanosecond units. The tick is only what the layer
     * reports to code that asks for one. */
    OS_SharedGlobalVars.MicroSecPerTick = OS_ATMOSPHERE_USEC_PER_TICK;
    OS_SharedGlobalVars.TicksPerSecond  = 1000000 / OS_ATMOSPHERE_USEC_PER_TICK;
    return OS_SUCCESS;
}

static void OS_Atmosphere_TimeBaseEntry(void *arg)
{
    OS_TimeBase_CallbackThread(OS_ObjectIdFromInteger((unsigned long)(uintptr_t)arg));
}

int32 OS_TimeBaseCreate_Impl(const OS_object_token_t *token)
{
    OS_impl_timebase_internal_record_t *impl     = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);
    OS_timebase_internal_record_t      *timebase = OS_OBJECT_TABLE_GET(OS_timebase_table, *token);
    uintptr_t                           id       = OS_ObjectIdToInteger(OS_ObjectIdFromToken(token));

    memset(impl, 0, sizeof(*impl));
    art_mutex_init(&impl->handler_mutex);
    art_sem_init(&impl->changed, 0, 1);
    impl->next_tick = ART_FOREVER;
    impl->simulate  = (timebase->external_sync == NULL);
    if (impl->simulate)
    {
        timebase->external_sync = OS_Atmosphere_TimeBaseWait;
    }

    /* The helper task is more urgent than its creator and runs at once. It
     * waits in the shared layer until the creator has finished the record. */
    if (art_task_create(&impl->handler_task, OS_Atmosphere_TimeBaseEntry, (void *)id, NULL,
                        OS_ATMOSPHERE_TIMEBASE_STACK_SIZE, OS_ATMOSPHERE_TIMEBASE_PRIORITY) != ART_OK)
    {
        art_sem_destroy(&impl->changed);
        art_mutex_destroy(&impl->handler_mutex);
        return OS_TIMER_ERR_INTERNAL;
    }
    return OS_SUCCESS;
}

int32 OS_TimeBaseSet_Impl(const OS_object_token_t *token, uint32 start_time, uint32 interval_time)
{
    OS_impl_timebase_internal_record_t *impl     = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);
    OS_timebase_internal_record_t      *timebase = OS_OBJECT_TABLE_GET(OS_timebase_table, *token);

    if (impl->simulate)
    {
        if (start_time == 0)
        {
            /* A zero start time stops the time base. */
            impl->start_usec    = 0;
            impl->interval_usec = 0;
            impl->next_tick     = ART_FOREVER;
        }
        else
        {
            impl->start_usec    = start_time;
            impl->interval_usec = interval_time;
            impl->next_tick     = art_clock_ns() + (uint64_t)start_time * 1000u;
            /* Ticks are exact to the clock, so the accuracy is one microsecond. */
            timebase->accuracy_usec = 1;
        }
        art_sem_give(&impl->changed);
    }
    impl->reset = true;
    return OS_SUCCESS;
}

int32 OS_TimeBaseDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_timebase_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);

    art_task_delete(impl->handler_task);
    art_sem_destroy(&impl->changed);
    art_mutex_destroy(&impl->handler_mutex);
    impl->handler_task = 0;
    impl->simulate     = false;
    return OS_SUCCESS;
}

int32 OS_TimeBaseGetInfo_Impl(const OS_object_token_t *token, OS_timebase_prop_t *timer_prop)
{
    (void)token;
    (void)timer_prop;
    return OS_SUCCESS;
}
