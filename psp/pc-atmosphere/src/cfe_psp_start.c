/*
 * Start of the flight software on Atmosphere.
 *
 * The BSP calls OS_Application_Startup(). The files of the non-volatile
 * volume /cf (the startup script and the table files) arrive as a tar archive
 * in the boot payload and become files of the runtime's in-memory file
 * system.
 */
#include <stdio.h>
#include <stdlib.h>

#include "common_types.h"
#include "osapi.h"

#include "cfe_psp.h"
#include "cfe_psp_memory.h"
#include "cfe_psp_module.h"
#include "target_config.h"

#include "atmo_rt.h"

#define CFE_PSP_MAIN_FUNCTION       (*GLOBAL_CONFIGDATA.CfeConfig->SystemMain)
#define CFE_PSP_NONVOL_STARTUP_FILE (GLOBAL_CONFIGDATA.CfeConfig->NonvolStartupFile)

/* The directory of the in-memory file system that holds the volume /cf. */
#define CFE_PSP_ATMOSPHERE_CF_DIR "/cf"

void CFE_PSP_Main(void);

void OS_Application_Startup(void)
{
    CFE_PSP_Main();
}

static void CFE_PSP_Atmosphere_LoadFiles(void)
{
    size_t      size = 0;
    const void *data = art_boot_payload(&size);
    int         count;

    if (size == 0)
    {
        OS_printf("CFE_PSP: No boot payload: the /cf volume is empty\n");
        return;
    }
    count = art_fs_add_tar(data, size);
    if (count < 0)
    {
        OS_printf("CFE_PSP: Cannot read the boot payload as a tar archive: %d\n", count);
    }
    else
    {
        OS_printf("CFE_PSP: %d files from the boot payload (%lu bytes)\n", count, (unsigned long)size);
    }
}

void CFE_PSP_Main(void)
{
    uint32    reset_type;
    uint32    reset_subtype;
    osal_id_t fs_id;
    int32     Status;

    Status = OS_API_Init();
    if (Status != OS_SUCCESS)
    {
        printf("CFE_PSP: OS_API_Init() failure\n");
        CFE_PSP_Panic(Status);
    }

    CFE_PSP_SetupReservedMemoryMap();

    CFE_PSP_Atmosphere_LoadFiles();
    Status = OS_FileSysAddFixedMap(&fs_id, CFE_PSP_ATMOSPHERE_CF_DIR, "/cf");
    if (Status != OS_SUCCESS)
    {
        OS_printf("CFE_PSP: OS_FileSysAddFixedMap() failure: %d\n", (int)Status);
    }

    CFE_PSP_ModuleInit();

    /* Memory does not survive a restart of the process, so every start is a
     * power-on reset. */
    reset_type    = CFE_PSP_RST_TYPE_POWERON;
    reset_subtype = CFE_PSP_RST_SUBTYPE_POWER_CYCLE;
    CFE_PSP_InitProcessorReservedMemory(reset_type);

    CFE_PSP_MAIN_FUNCTION(reset_type, reset_subtype, 1, CFE_PSP_NONVOL_STARTUP_FILE);
}
