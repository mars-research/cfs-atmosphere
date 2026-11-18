/*
 * The console. With OSAL_CONFIG_CONSOLE_ASYNC, a helper task writes the
 * output; otherwise OS_printf() writes it in the calling task.
 */
#include "os-atmosphere.h"
#include "os-impl-console.h"
#include "os-shared-printf.h"
#include "os-shared-idmap.h"
#include "os-shared-common.h"

#define OS_CONSOLE_TASK_PRIORITY  OS_UTILITYTASK_PRIORITY
#define OS_CONSOLE_TASK_STACKSIZE OS_UTILITYTASK_STACK_SIZE

OS_impl_console_internal_record_t OS_impl_console_table[OS_MAX_CONSOLES];

void OS_ConsoleWakeup_Impl(const OS_object_token_t *token)
{
    OS_impl_console_internal_record_t *local = OS_OBJECT_TABLE_GET(OS_impl_console_table, *token);

    art_sem_give(&local->data_sem);
}

static void OS_Atmosphere_ConsoleTask(void *arg)
{
    OS_object_token_t                  token;
    OS_impl_console_internal_record_t *local;

    if (OS_ObjectIdGetById(OS_LOCK_MODE_REFCOUNT, OS_OBJECT_TYPE_OS_CONSOLE,
                           OS_ObjectIdFromInteger((unsigned long)(uintptr_t)arg), &token) == OS_SUCCESS)
    {
        local = OS_OBJECT_TABLE_GET(OS_impl_console_table, token);
        while (OS_SharedGlobalVars.GlobalState != OS_SHUTDOWN_MAGIC_NUMBER)
        {
            OS_ConsoleOutput_Impl(&token);
            art_sem_take(&local->data_sem, ART_FOREVER);
        }
        OS_ObjectIdRelease(&token);
    }
}

int32 OS_ConsoleCreate_Impl(const OS_object_token_t *token)
{
    OS_impl_console_internal_record_t *local   = OS_OBJECT_TABLE_GET(OS_impl_console_table, *token);
    OS_console_internal_record_t      *console = OS_OBJECT_TABLE_GET(OS_console_table, *token);
    uintptr_t                          id      = OS_ObjectIdToInteger(OS_ObjectIdFromToken(token));

    if (OS_ObjectIndexFromToken(token) != 0)
    {
        return OS_ERR_NOT_IMPLEMENTED;
    }
    if (console->IsAsync)
    {
        art_sem_init(&local->data_sem, 0, 1);
        if (art_task_create(&local->task, OS_Atmosphere_ConsoleTask, (void *)id, NULL, OS_CONSOLE_TASK_STACKSIZE,
                            OS_CONSOLE_TASK_PRIORITY) != ART_OK)
        {
            art_sem_destroy(&local->data_sem);
            return OS_ERROR;
        }
    }
    return OS_SUCCESS;
}
