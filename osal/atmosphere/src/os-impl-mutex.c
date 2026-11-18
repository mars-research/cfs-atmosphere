/*
 * Mutexes. They are recursive and the owner inherits the priority of the
 * most urgent task that waits.
 */
#include "os-atmosphere.h"
#include "os-impl-mutex.h"
#include "os-shared-mutex.h"
#include "os-shared-idmap.h"

OS_impl_mutex_internal_record_t OS_impl_mutex_table[OS_MAX_MUTEXES];

int32 OS_Atmosphere_MutexAPI_Impl_Init(void)
{
    memset(OS_impl_mutex_table, 0, sizeof(OS_impl_mutex_table));
    return OS_SUCCESS;
}

int32 OS_MutSemCreate_Impl(const OS_object_token_t *token, uint32 options)
{
    OS_impl_mutex_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    (void)options;
    art_mutex_init(&impl->mutex);
    return OS_SUCCESS;
}

int32 OS_MutSemDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_mutex_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    art_mutex_destroy(&impl->mutex);
    return OS_SUCCESS;
}

int32 OS_MutSemGive_Impl(const OS_object_token_t *token)
{
    OS_impl_mutex_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    if (art_mutex_unlock(&impl->mutex) != ART_OK)
    {
        return OS_SEM_FAILURE;
    }
    return OS_SUCCESS;
}

int32 OS_MutSemTake_Impl(const OS_object_token_t *token)
{
    OS_impl_mutex_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    if (art_mutex_lock(&impl->mutex) != ART_OK)
    {
        return OS_SEM_FAILURE;
    }
    return OS_SUCCESS;
}

int32 OS_MutSemGetInfo_Impl(const OS_object_token_t *token, OS_mut_sem_prop_t *mut_prop)
{
    (void)token;
    (void)mut_prop;
    return OS_SUCCESS;
}
