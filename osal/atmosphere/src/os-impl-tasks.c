/*
 * Tasks.
 */
#include "os-atmosphere.h"
#include "os-impl-tasks.h"
#include "os-shared-task.h"
#include "os-shared-task-affinity.h"
#include "os-shared-idmap.h"
#include "os-shared-common.h"

OS_impl_task_internal_record_t OS_impl_task_table[OS_MAX_TASKS];

/* The argument is the OSAL ID of the task. */
static void OS_Atmosphere_TaskEntry(void *arg)
{
    OS_TaskEntryPoint(OS_ObjectIdFromInteger((unsigned long)(uintptr_t)arg));
}

int32 OS_Atmosphere_TaskAPI_Impl_Init(void)
{
    memset(OS_impl_task_table, 0, sizeof(OS_impl_task_table));
    OS_SharedGlobalVars.CoresConfigured = OS_TaskAffinityGetCoresConfigured_Impl();
    return OS_SUCCESS;
}

int32 OS_TaskCreate_Impl(const OS_object_token_t *token, uint32 flags)
{
    OS_impl_task_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);
    OS_task_internal_record_t      *task = OS_OBJECT_TABLE_GET(OS_task_table, *token);
    uintptr_t                       id   = OS_ObjectIdToInteger(OS_ObjectIdFromToken(token));
    int                             status;

    /* The runtime saves no floating-point state, so OS_FP_ENABLED has no effect. */
    (void)flags;

    /* A more urgent new task runs before this call returns. The runtime
     * stores the handle first, which OS_TaskMatch_Impl needs. */
    status = art_task_create(&impl->handle, OS_Atmosphere_TaskEntry, (void *)id, task->stack_pointer,
                             task->stack_size, task->priority);
    if (status != ART_OK)
    {
        OS_DEBUG("art_task_create failed: %d\n", status);
        return OS_ERROR;
    }
    return OS_SUCCESS;
}

int32 OS_TaskDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_task_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    /* The task may have ended by itself already. */
    art_task_delete(impl->handle);
    impl->handle = 0;
    return OS_SUCCESS;
}

int32 OS_TaskDetach_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_SUCCESS;
}

void OS_TaskExit_Impl(void)
{
    art_task_exit();
}

int32 OS_TaskDelay_Impl(uint32 millisecond)
{
    art_task_sleep((uint64_t)millisecond * 1000000u);
    return OS_SUCCESS;
}

int32 OS_TaskSetPriority_Impl(const OS_object_token_t *token, osal_priority_t new_priority)
{
    OS_impl_task_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    if (art_task_set_priority(impl->handle, new_priority) != ART_OK)
    {
        return OS_ERROR;
    }
    return OS_SUCCESS;
}

int32 OS_TaskMatch_Impl(const OS_object_token_t *token)
{
    OS_impl_task_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    return (art_task_self() == impl->handle) ? OS_SUCCESS : OS_ERROR;
}

/* The OSAL ID of a task is kept as the tag of its runtime task. */
int32 OS_TaskRegister_Impl(osal_id_t global_task_id)
{
    if (art_task_set_tag(art_task_self(), OS_ObjectIdToInteger(global_task_id)) != ART_OK)
    {
        return OS_ERROR;
    }
    return OS_SUCCESS;
}

osal_id_t OS_TaskGetId_Impl(void)
{
    /* The tag is zero, which is OS_OBJECT_ID_UNDEFINED, for a task that
     * the OSAL did not create. */
    return OS_ObjectIdFromInteger((unsigned long)art_task_get_tag(art_task_self()));
}

int32 OS_TaskGetInfo_Impl(const OS_object_token_t *token, OS_task_prop_t *task_prop)
{
    (void)token;
    (void)task_prop;
    return OS_SUCCESS;
}

int32 OS_TaskValidateSystemData_Impl(const void *sysdata, size_t sysdata_size)
{
    if (sysdata == NULL || sysdata_size != sizeof(art_task_t))
    {
        return OS_INVALID_POINTER;
    }
    return OS_SUCCESS;
}

bool OS_TaskIdMatchSystemData_Impl(void *ref, const OS_object_token_t *token, const OS_common_record_t *obj)
{
    const art_task_t               *target = (const art_task_t *)ref;
    OS_impl_task_internal_record_t *impl   = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    (void)obj;
    return (*target == impl->handle);
}

/* The runtime runs all tasks on one processor. */
uint32 OS_TaskAffinityGetCoresConfigured_Impl(void)
{
    return 1;
}

int32 OS_TaskAffinitySetAffinity_Impl(const OS_object_token_t *token, const OS_cpuset_t cpuset)
{
    (void)token;
    (void)cpuset;
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_TaskAffinityGetAffinity_Impl(const OS_object_token_t *token, OS_cpuset_t *cpuset)
{
    (void)token;
    (void)cpuset;
    return OS_ERR_NOT_IMPLEMENTED;
}
