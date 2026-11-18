/*
 * The program entry of the Atmosphere BSP.
 *
 * The Atmosphere C runtime owns the process entry and calls main().
 */
#include <stdlib.h>
#include <string.h>

#include "atmo_rt.h"
#include "bsp-impl.h"

static art_mutex_t OS_BSP_Atmosphere_Lock;

void OS_BSP_Lock_Impl(void)
{
    art_mutex_lock(&OS_BSP_Atmosphere_Lock);
}

void OS_BSP_Unlock_Impl(void)
{
    art_mutex_unlock(&OS_BSP_Atmosphere_Lock);
}

void OS_BSP_Shutdown_Impl(void)
{
    abort();
}

int main(void)
{
    memset(&OS_BSP_Global, 0, sizeof(OS_BSP_Global));
    art_mutex_init(&OS_BSP_Atmosphere_Lock);

    /* Sets up all tasks and resources of the application, then returns. */
    OS_Application_Startup();

    /* The background of the application; the default calls OS_IdleLoop(). */
    OS_Application_Run();

    /* The runtime ends the program with this value. */
    return (OS_BSP_Global.AppStatus == OS_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
}
