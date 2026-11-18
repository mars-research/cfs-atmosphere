/*
 * Configuration of the Atmosphere PSP.
 */
#ifndef CFE_PSP_CONFIG_H
#define CFE_PSP_CONFIG_H

#include "common_types.h"

/* The size of the memory range table. */
#define CFE_PSP_MEM_TABLE_SIZE 10

/* The kernel does not report task exceptions to the process, so the log is
 * never written. One entry keeps the shared code well formed. */
#define CFE_PSP_MAX_EXCEPTION_ENTRIES 1

/* The period of the "cFS-Master" time base in microseconds. */
#define CFE_PSP_SOFT_TIMEBASE_PERIOD 10000

#define CFE_PSP_BOOTRECORD_VALID   ((uint32)0x2aebe984)
#define CFE_PSP_BOOTRECORD_INVALID (~CFE_PSP_BOOTRECORD_VALID)

/* The boot record, at the start of the reserved memory. */
typedef struct
{
    uint32 ValidityFlag;
    uint32 NextResetType;
} CFE_PSP_ReservedMemoryBootRecord_t;

/* The native task handle: a task handle of the Atmosphere C runtime. */
typedef uint32 CFE_PSP_Exception_SysTaskId_t;

typedef struct
{
    uint32 reserved; /* prevent empty structure */
} CFE_PSP_Exception_ContextDataEntry_t;

#define CFE_PSP_WATCHDOG_MIN (0)
#define CFE_PSP_WATCHDOG_MAX (0xFFFFFFFF)

#define CFE_PSP_NUM_EEPROM_BANKS 1

/* The alignment of each part of the reserved memory. */
#define CFE_PSP_MEMALIGN_MASK ((cpuaddr)0x7F)

#endif /* CFE_PSP_CONFIG_H */
