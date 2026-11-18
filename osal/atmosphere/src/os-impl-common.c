/*
 * Initialization, the idle loop, and shutdown.
 */
#include "os-atmosphere.h"
#include "os-shared-common.h"
#include "os-shared-idmap.h"

/* The thread that called OS_IdleLoop() waits on this until shutdown. */
static art_sem_t OS_Atmosphere_IdleSem;
static bool      OS_Atmosphere_IdleSemReady;

int32 OS_API_Impl_Init(osal_objtype_t idtype)
{
    int32 return_code;

    if (!OS_Atmosphere_IdleSemReady)
    {
        art_sem_init(&OS_Atmosphere_IdleSem, 0, 1);
        OS_Atmosphere_IdleSemReady = true;
    }

    return_code = OS_Atmosphere_TableMutex_Init(idtype);
    if (return_code != OS_SUCCESS)
    {
        return return_code;
    }

    switch (idtype)
    {
        case OS_OBJECT_TYPE_OS_TASK:
            return_code = OS_Atmosphere_TaskAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_QUEUE:
            return_code = OS_Atmosphere_QueueAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_BINSEM:
            return_code = OS_Atmosphere_BinSemAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_COUNTSEM:
            return_code = OS_Atmosphere_CountSemAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_MUTEX:
            return_code = OS_Atmosphere_MutexAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_TIMEBASE:
            return_code = OS_Atmosphere_TimeBaseAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_STREAM:
            return_code = OS_Atmosphere_StreamAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_DIR:
            return_code = OS_Atmosphere_DirAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_FILESYS:
            return_code = OS_Atmosphere_FileSysAPI_Impl_Init();
            break;
        default:
            break;
    }

    return return_code;
}

void OS_IdleLoop_Impl(void)
{
    art_sem_take(&OS_Atmosphere_IdleSem, ART_FOREVER);
}

void OS_ApplicationShutdown_Impl(void)
{
    art_sem_give(&OS_Atmosphere_IdleSem);
}
