/*
 * Definitions shared by the files of the Atmosphere OSAL layer.
 *
 * The layer maps each OSAL object onto an object of the Atmosphere C runtime
 * (atmo_rt.h). OSAL priorities are runtime priorities: 0 is the most urgent.
 */
#ifndef OS_ATMOSPHERE_H
#define OS_ATMOSPHERE_H

#include <stdint.h>
#include <string.h>

#include "atmo_rt.h"

#include "osconfig.h"
#include "common_types.h"
#include "osapi.h"
#include "os-shared-globaldefs.h"

/* The helper tasks of time bases run before every OSAL task. */
#define OS_ATMOSPHERE_TIMEBASE_PRIORITY 0

/* The tick that the layer reports. Waits are not rounded to it. */
#define OS_ATMOSPHERE_USEC_PER_TICK 1000

/* The deadline that is `msecs` milliseconds from now. */
static inline uint64_t OS_Atmosphere_Deadline(uint32 msecs)
{
    return art_deadline_after((uint64_t)msecs * 1000000u);
}

/* Maps a result of the runtime's file system to an OSAL status. */
int32 OS_Atmosphere_FsStatus(int64_t result);

int32 OS_Atmosphere_TableMutex_Init(osal_objtype_t idtype);
int32 OS_Atmosphere_TaskAPI_Impl_Init(void);
int32 OS_Atmosphere_QueueAPI_Impl_Init(void);
int32 OS_Atmosphere_BinSemAPI_Impl_Init(void);
int32 OS_Atmosphere_CountSemAPI_Impl_Init(void);
int32 OS_Atmosphere_MutexAPI_Impl_Init(void);
int32 OS_Atmosphere_TimeBaseAPI_Impl_Init(void);
int32 OS_Atmosphere_StreamAPI_Impl_Init(void);
int32 OS_Atmosphere_DirAPI_Impl_Init(void);
int32 OS_Atmosphere_FileSysAPI_Impl_Init(void);

#endif /* OS_ATMOSPHERE_H */
