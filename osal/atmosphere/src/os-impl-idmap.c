/*
 * The lock of each object table.
 */
#include "os-atmosphere.h"
#include "os-shared-idmap.h"
#include "os-impl-idmap.h"

static OS_impl_objtype_lock_t OS_task_table_lock;
static OS_impl_objtype_lock_t OS_queue_table_lock;
static OS_impl_objtype_lock_t OS_bin_sem_table_lock;
static OS_impl_objtype_lock_t OS_mutex_table_lock;
static OS_impl_objtype_lock_t OS_count_sem_table_lock;
static OS_impl_objtype_lock_t OS_stream_table_lock;
static OS_impl_objtype_lock_t OS_dir_table_lock;
static OS_impl_objtype_lock_t OS_timebase_table_lock;
static OS_impl_objtype_lock_t OS_timecb_table_lock;
static OS_impl_objtype_lock_t OS_module_table_lock;
static OS_impl_objtype_lock_t OS_filesys_table_lock;
static OS_impl_objtype_lock_t OS_console_lock;
static OS_impl_objtype_lock_t OS_condvar_lock;
static OS_impl_objtype_lock_t OS_rwlock_lock;

OS_impl_objtype_lock_t *const OS_impl_objtype_lock_table[OS_OBJECT_TYPE_USER] = {
    [OS_OBJECT_TYPE_UNDEFINED]   = NULL,
    [OS_OBJECT_TYPE_OS_TASK]     = &OS_task_table_lock,
    [OS_OBJECT_TYPE_OS_QUEUE]    = &OS_queue_table_lock,
    [OS_OBJECT_TYPE_OS_COUNTSEM] = &OS_count_sem_table_lock,
    [OS_OBJECT_TYPE_OS_BINSEM]   = &OS_bin_sem_table_lock,
    [OS_OBJECT_TYPE_OS_MUTEX]    = &OS_mutex_table_lock,
    [OS_OBJECT_TYPE_OS_STREAM]   = &OS_stream_table_lock,
    [OS_OBJECT_TYPE_OS_DIR]      = &OS_dir_table_lock,
    [OS_OBJECT_TYPE_OS_TIMEBASE] = &OS_timebase_table_lock,
    [OS_OBJECT_TYPE_OS_TIMECB]   = &OS_timecb_table_lock,
    [OS_OBJECT_TYPE_OS_MODULE]   = &OS_module_table_lock,
    [OS_OBJECT_TYPE_OS_FILESYS]  = &OS_filesys_table_lock,
    [OS_OBJECT_TYPE_OS_CONSOLE]  = &OS_console_lock,
    [OS_OBJECT_TYPE_OS_CONDVAR]  = &OS_condvar_lock,
    [OS_OBJECT_TYPE_OS_RWLOCK]   = &OS_rwlock_lock,
};

void OS_Lock_Global_Impl(osal_objtype_t idtype)
{
    OS_impl_objtype_lock_t *impl = OS_impl_objtype_lock_table[idtype];

    if (impl != NULL)
    {
        art_mutex_lock(&impl->mutex);
    }
}

void OS_Unlock_Global_Impl(osal_objtype_t idtype)
{
    OS_impl_objtype_lock_t *impl = OS_impl_objtype_lock_table[idtype];

    if (impl != NULL)
    {
        art_mutex_unlock(&impl->mutex);
    }
}

/*
 * The caller holds the table lock and waits for another task to change an
 * entry. The wait grows with each attempt, as in the RTEMS layer.
 */
void OS_WaitForStateChange_Impl(osal_objtype_t objtype, uint32 attempts)
{
    uint32 wait_ms = (attempts <= 10) ? attempts * attempts : 100;

    OS_Unlock_Global_Impl(objtype);
    art_task_sleep((uint64_t)wait_ms * 1000000u);
    OS_Lock_Global_Impl(objtype);
}

int32 OS_Atmosphere_TableMutex_Init(osal_objtype_t idtype)
{
    OS_impl_objtype_lock_t *impl = OS_impl_objtype_lock_table[idtype];

    if (impl != NULL)
    {
        art_mutex_init(&impl->mutex);
    }
    return OS_SUCCESS;
}
