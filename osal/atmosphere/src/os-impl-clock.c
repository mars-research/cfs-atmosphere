/*
 * Clocks. The monotonic clock is the runtime clock. The local clock is the
 * monotonic clock plus an offset that OS_SetLocalTime() changes.
 */
#include "os-atmosphere.h"
#include "os-shared-clock.h"

static int64 OS_Atmosphere_LocalOffsetNsec;

int64 OS_Atmosphere_LocalTimeOf(uint64_t monotonic_nsec)
{
    return (int64)monotonic_nsec + OS_Atmosphere_LocalOffsetNsec;
}

int32 OS_GetMonotonicTime_Impl(OS_time_t *time_struct)
{
    *time_struct = OS_TimeFromTotalNanoseconds((int64)art_clock_ns());
    return OS_SUCCESS;
}

int32 OS_GetLocalTime_Impl(OS_time_t *time_struct)
{
    *time_struct = OS_TimeFromTotalNanoseconds(OS_Atmosphere_LocalTimeOf(art_clock_ns()));
    return OS_SUCCESS;
}

int32 OS_SetLocalTime_Impl(const OS_time_t *time_struct)
{
    OS_Atmosphere_LocalOffsetNsec = OS_TimeGetTotalNanoseconds(*time_struct) - (int64)art_clock_ns();
    return OS_SUCCESS;
}
