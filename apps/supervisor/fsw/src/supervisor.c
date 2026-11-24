/*
 * SUPERVISOR: the loader and watchdog of the demo's containers.
 *
 * At start it loads the bus and the payload from the /cf volume, each into
 * its own child container (demo/mc/demo_mc.c). It then watches them:
 *
 *   - a child whose heartbeat has not moved for SUPERVISOR_STALL_MS is
 *     killed and started again from its image;
 *   - the same happens to the payload when the bus asks for it
 *     (RESTART_REQUEST), after the payload has broken the bus's policy.
 *
 * No operator takes part. Every step is logged with the timestamp counter,
 * so that it can be lined up with the lines of the bus and the payload.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "cfe.h"

#include "atmo_rt.h"
#include "demo_mc.h"
#include "demo_msg.h"

#define SUPERVISOR_PIPE_DEPTH 16
#define SUPERVISOR_POLL_MS    20
#define SUPERVISOR_STALL_MS   500
/* Starts of one child in a row that may fail before SUPERVISOR gives up. */
#define SUPERVISOR_MAX_FAILURES 3

static const char *const SUPERVISOR_Image[DEMO_CHILDREN] = {"/cf/bus.elf", "/cf/payload.elf"};

typedef struct
{
    uint64 Beat;     /* the heartbeat last seen */
    uint64 SeenAtNs; /* when it last moved; zero right after a start */
    uint32 Failures;
    uint32 Restarts;
} SUPERVISOR_Watch_t;

static SUPERVISOR_Watch_t SUPERVISOR_Watch[DEMO_CHILDREN];

void demo_mc_log(const char *fmt, ...)
{
    char    Line[240];
    va_list Args;

    va_start(Args, fmt);
    vsnprintf(Line, sizeof(Line), fmt, Args);
    va_end(Args);
    OS_printf("%s", Line);
}

static void SUPERVISOR_Start(int Which)
{
    SUPERVISOR_Watch_t *W = &SUPERVISOR_Watch[Which];

    if (W->Failures >= SUPERVISOR_MAX_FAILURES)
    {
        return;
    }
    if (demo_mc_start(Which, SUPERVISOR_Image[Which]) == 0)
    {
        W->Failures = 0;
    }
    else
    {
        W->Failures++;
        if (W->Failures == SUPERVISOR_MAX_FAILURES)
        {
            OS_printf("SUPERVISOR: giving up on %s after %u failed starts\n", demo_mc_name(Which),
                      (unsigned int)W->Failures);
        }
    }
    W->SeenAtNs = 0;
}

static void SUPERVISOR_Restart(int Which, const char *Why)
{
    uint64 T0 = art_clock_ns();

    OS_printf("SUPERVISOR: restarting %s: %s, tsc %lu\n", demo_mc_name(Which), Why,
              (unsigned long)art_sys_rdtsc());
    if (Which == DEMO_CHILD_BUS)
    {
        /* A new bus starts every ring from empty, so the payload, which
         * has counts of its own, starts again too. */
        demo_mc_stop(DEMO_CHILD_PAYLOAD);
        demo_mc_stop(DEMO_CHILD_BUS);
        SUPERVISOR_Start(DEMO_CHILD_BUS);
        SUPERVISOR_Start(DEMO_CHILD_PAYLOAD);
    }
    else
    {
        demo_mc_stop(Which);
        SUPERVISOR_Start(Which);
    }
    SUPERVISOR_Watch[Which].Restarts++;
    OS_printf("SUPERVISOR: %s restart %u done in %lu us, tsc %lu\n", demo_mc_name(Which),
              (unsigned int)SUPERVISOR_Watch[Which].Restarts, (unsigned long)((art_clock_ns() - T0) / 1000),
              (unsigned long)art_sys_rdtsc());
}

static void SUPERVISOR_CheckHeartbeats(void)
{
    uint64 Now = art_clock_ns();
    int    Which;

    for (Which = 0; Which < DEMO_CHILDREN; Which++)
    {
        SUPERVISOR_Watch_t *W    = &SUPERVISOR_Watch[Which];
        uint64              Beat = demo_mc_heartbeat(Which);

        if (!demo_mc_running(Which))
        {
            continue;
        }
        if (Beat != W->Beat || W->SeenAtNs == 0)
        {
            W->Beat     = Beat;
            W->SeenAtNs = Now;
        }
        else if (Now - W->SeenAtNs >= SUPERVISOR_STALL_MS * 1000000ull)
        {
            OS_printf("SUPERVISOR: detected %s heartbeat stalled for %lu ms, tsc %lu\n", demo_mc_name(Which),
                      (unsigned long)((Now - W->SeenAtNs) / 1000000ull), (unsigned long)art_sys_rdtsc());
            SUPERVISOR_Restart(Which, "its heartbeat stalled");
        }
    }
}

static void SUPERVISOR_Command(const CFE_SB_Buffer_t *Buf)
{
    struct demo_restart_request Req;
    CFE_MSG_Size_t              Size = 0;
    CFE_MSG_FcnCode_t           Cc   = 0;

    CFE_MSG_GetSize(&Buf->Msg, &Size);
    CFE_MSG_GetFcnCode(&Buf->Msg, &Cc);
    if (Cc != DEMO_SUPERVISOR_RESTART_REQUEST_CC || Size != DEMO_CMD_HDR + sizeof(Req))
    {
        OS_printf("SUPERVISOR: ignored command cc=%u len=%u\n", (unsigned int)Cc, (unsigned int)Size);
        return;
    }
    memcpy(&Req, (const uint8 *)Buf + DEMO_CMD_HDR, sizeof(Req));
    OS_printf("SUPERVISOR: detected RESTART_REQUEST from the bus: process=%u reason=%u denials=%u dropped=%u, tsc %lu\n",
              (unsigned int)Req.process, (unsigned int)Req.reason, (unsigned int)Req.denials,
              (unsigned int)Req.dropped, (unsigned long)art_sys_rdtsc());
    /* The bus may only ask for the payload. */
    if (Req.process == DEMO_PROC_PAYLOAD && demo_mc_running(DEMO_CHILD_PAYLOAD))
    {
        SUPERVISOR_Restart(DEMO_CHILD_PAYLOAD, "the bus asked for it");
    }
}

void SUPERVISOR_AppMain(void)
{
    uint32           RunStatus = CFE_ES_RunStatus_APP_RUN;
    CFE_SB_PipeId_t  Pipe;
    CFE_SB_Buffer_t *Buf;
    CFE_Status_t     Status;

    CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    Status = CFE_SB_CreatePipe(&Pipe, SUPERVISOR_PIPE_DEPTH, "SUPERVISOR_PIPE");
    if (Status == CFE_SUCCESS)
    {
        Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(DEMO_SUPERVISOR_CMD_MID), Pipe);
    }
    if (Status != CFE_SUCCESS || demo_mc_init() != 0)
    {
        CFE_ES_WriteToSysLog("SUPERVISOR: cannot start, RC = 0x%08lX\n", (unsigned long)Status);
        RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }
    else
    {
        OS_printf("SUPERVISOR: started, tsc %lu, counter %lu Hz\n", (unsigned long)art_sys_rdtsc(),
                  (unsigned long)art_clock_counter_hz());
        SUPERVISOR_Start(DEMO_CHILD_BUS);
        SUPERVISOR_Start(DEMO_CHILD_PAYLOAD);
    }

    while (CFE_ES_RunLoop(&RunStatus))
    {
        Status = CFE_SB_ReceiveBuffer(&Buf, Pipe, SUPERVISOR_POLL_MS);
        if (Status == CFE_SUCCESS)
        {
            SUPERVISOR_Command(Buf);
        }
        else if (Status != CFE_SB_TIME_OUT)
        {
            CFE_ES_WriteToSysLog("SUPERVISOR: pipe read error, RC = 0x%08lX\n", (unsigned long)Status);
            RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
        SUPERVISOR_CheckHeartbeats();
    }
    CFE_ES_ExitApp(RunStatus);
}
