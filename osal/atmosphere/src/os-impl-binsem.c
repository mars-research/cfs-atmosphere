/*
 * Binary semaphores.
 */
#include "os-atmosphere.h"
#include "os-impl-binsem.h"
#include "os-shared-binsem.h"
#include "os-shared-idmap.h"

OS_impl_binsem_internal_record_t OS_impl_bin_sem_table[OS_MAX_BIN_SEMAPHORES];

int32 OS_Atmosphere_BinSemAPI_Impl_Init(void)
{
    memset(OS_impl_bin_sem_table, 0, sizeof(OS_impl_bin_sem_table));
    return OS_SUCCESS;
}

int32 OS_BinSemCreate_Impl(const OS_object_token_t *token, uint32 sem_initial_value, uint32 options)
{
    OS_impl_binsem_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    (void)options;
    art_sem_init(&impl->sem, sem_initial_value > 0 ? 1 : 0, 1);
    return OS_SUCCESS;
}

int32 OS_BinSemDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_binsem_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    art_sem_destroy(&impl->sem);
    return OS_SUCCESS;
}

int32 OS_BinSemGive_Impl(const OS_object_token_t *token)
{
    OS_impl_binsem_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    /* Giving a semaphore that is already available leaves it available. */
    art_sem_give(&impl->sem);
    return OS_SUCCESS;
}

int32 OS_BinSemFlush_Impl(const OS_object_token_t *token)
{
    OS_impl_binsem_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    art_sem_flush(&impl->sem);
    return OS_SUCCESS;
}

static int32 OS_Atmosphere_BinSemWait(const OS_object_token_t *token, uint64_t deadline)
{
    OS_impl_binsem_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

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

int32 OS_BinSemTake_Impl(const OS_object_token_t *token)
{
    return OS_Atmosphere_BinSemWait(token, ART_FOREVER);
}

int32 OS_BinSemTimedWait_Impl(const OS_object_token_t *token, uint32 msecs)
{
    return OS_Atmosphere_BinSemWait(token, msecs == 0 ? ART_NO_WAIT : OS_Atmosphere_Deadline(msecs));
}

int32 OS_BinSemGetInfo_Impl(const OS_object_token_t *token, OS_bin_sem_prop_t *bin_prop)
{
    OS_impl_binsem_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    bin_prop->value = (int32)art_sem_count(&impl->sem);
    return OS_SUCCESS;
}
