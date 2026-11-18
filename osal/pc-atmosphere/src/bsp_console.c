/*
 * Console output of the Atmosphere BSP.
 */
#include "atmo_rt.h"
#include "bsp-impl.h"

void OS_BSP_ConsoleOutput_Impl(const char *Str, size_t DataLen)
{
    art_console_write(Str, DataLen);
}

/* The kernel console has no text attributes. */
void OS_BSP_ConsoleSetMode_Impl(uint32 ModeBits)
{
    (void)ModeBits;
}
