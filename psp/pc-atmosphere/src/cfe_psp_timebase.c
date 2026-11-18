/*
 * The cFE time base: the monotonic clock of the Atmosphere C runtime.
 */
#include "cfe_psp.h"

#include "atmo_rt.h"

#define CFE_PSP_ATMOSPHERE_NSEC_PER_SEC 1000000000u

/* The upper word counts seconds and the lower word nanoseconds. */
void CFE_PSP_Get_Timebase(uint32 *Tbu, uint32 *Tbl)
{
    uint64_t now = art_clock_ns();

    *Tbu = (uint32)(now / CFE_PSP_ATMOSPHERE_NSEC_PER_SEC);
    *Tbl = (uint32)(now % CFE_PSP_ATMOSPHERE_NSEC_PER_SEC);
}

void CFE_PSP_GetTime(OS_time_t *LocalTime)
{
    *LocalTime = OS_TimeFromTotalNanoseconds((int64)art_clock_ns());
}

uint32 CFE_PSP_GetTimerTicksPerSecond(void)
{
    return CFE_PSP_ATMOSPHERE_NSEC_PER_SEC;
}

uint32 CFE_PSP_GetTimerLow32Rollover(void)
{
    return CFE_PSP_ATMOSPHERE_NSEC_PER_SEC;
}
