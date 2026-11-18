/*
 * Counting semaphores.
 */
#include "os-atmosphere.h"
#include "os-impl-countsem.h"
#include "os-shared-countsem.h"
#include "os-shared-idmap.h"

#define OS_ATMOSPHERE_COUNTSEM_MAX 0x7FFFFFFFu

OS_impl_countsem_internal_record_t OS_impl_count_sem_table[OS_MAX_COUNT_SEMAPHORES];

int32 OS_Atmosphere_CountSemAPI_Impl_Init(void)
{
    memset(OS_impl_count_sem_table, 0, sizeof(OS_impl_count_sem_table));
    return OS_SUCCESS;
}

int32 OS_CountSemCreate_Impl(const OS_object_token_t *token, uint32 sem_initial_value, uint32 options)
{
    OS_impl_countsem_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token);

    (void)options;
    if (sem_initial_value > OS_ATMOSPHERE_COUNTSEM_MAX)
    {
        return OS_INVALID_SEM_VALUE;
    }
    art_sem_init(&impl->sem, sem_initial_value, OS_ATMOSPHERE_COUNTSEM_MAX);
    return OS_SUCCESS;
}

int32 OS_CountSemDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_countsem_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token);

    art_sem_destroy(&impl->sem);
    return OS_SUCCESS;
}

int32 OS_CountSemGive_Impl(const OS_object_token_t *token)
{
    OS_impl_countsem_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token);

    if (art_sem_give(&impl->sem) != ART_OK)
    {
        return OS_SEM_FAILURE;
    }
    return OS_SUCCESS;
}

static int32 OS_Atmosphere_CountSemWait(const OS_object_token_t *token, uint64_t deadline)
{
    OS_impl_countsem_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token);

    switch (art_sem_take(&impl->sem, deadline))
    {
        case ART_OK:
            return OS_SUCCESS;
        case ART_TIMEOUT:
            return OS_SEM_TIMEOUT;
        default:
            return OS_SEM_FAILURE;
    }
}

int32 OS_CountSemTake_Impl(const OS_object_token_t *token)
{
    return OS_Atmosphere_CountSemWait(token, ART_FOREVER);
}

int32 OS_CountSemTimedWait_Impl(const OS_object_token_t *token, uint32 msecs)
{
    return OS_Atmosphere_CountSemWait(token, msecs == 0 ? ART_NO_WAIT : OS_Atmosphere_Deadline(msecs));
}

int32 OS_CountSemGetInfo_Impl(const OS_object_token_t *token, OS_count_sem_prop_t *count_prop)
{
    OS_impl_countsem_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token);

    count_prop->value = (int32)art_sem_count(&impl->sem);
    return OS_SUCCESS;
}
