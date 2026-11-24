/*
 * ADCS_SIM: a stand-in for attitude control in the mission-critical process.
 *
 * It accepts SET_POINTING with three angles in millidegrees (integers; the
 * port has no floating point) and reports the last accepted pointing once a
 * second. It also reports the largest angle it ever accepted: the bus is
 * supposed to keep requests of the payload within DEMO_ADCS_MAX_MILLIDEG,
 * and this number shows whether one got through.
 *
 * The application itself accepts any angle. The limit is the policy of the
 * bus, not a check here, so that the demo shows where the limit is enforced.
 */
#include <string.h>

#include "cfe.h"

#include "demo_msg.h"

#define ADCS_SIM_PIPE_DEPTH 32
#define ADCS_SIM_HK_MS      1000

static struct
{
    CFE_MSG_TelemetryHeader_t Hdr;
    struct demo_adcs_hk       Hk;
} ADCS_SIM_HkMsg;

static void ADCS_SIM_Command(const CFE_SB_Buffer_t *Buf)
{
    struct demo_adcs_hk     *Hk   = &ADCS_SIM_HkMsg.Hk;
    CFE_MSG_Size_t           Size = 0;
    CFE_MSG_FcnCode_t        Cc   = 0;
    struct demo_pointing_cmd P;
    int                      i;

    CFE_MSG_GetSize(&Buf->Msg, &Size);
    CFE_MSG_GetFcnCode(&Buf->Msg, &Cc);
    switch (Cc)
    {
        case DEMO_ADCS_NOOP_CC:
            Hk->cmd_count++;
            break;
        case DEMO_ADCS_RESET_CC:
            OS_printf("ADCS_SIM: RESET: pointing cleared\n");
            Hk->roll = Hk->pitch = Hk->yaw = 0;
            Hk->cmd_count++;
            break;
        case DEMO_ADCS_SET_POINTING_CC:
            if (Size != DEMO_CMD_HDR + sizeof(P))
            {
                Hk->err_count++;
                break;
            }
            memcpy(&P, (const uint8 *)Buf + DEMO_CMD_HDR, sizeof(P));
            Hk->roll  = P.roll;
            Hk->pitch = P.pitch;
            Hk->yaw   = P.yaw;
            Hk->accepted++;
            Hk->cmd_count++;
            for (i = 0; i < 3; i++)
            {
                int32 v = (&P.roll)[i];
                if (v < 0)
                {
                    v = -v;
                }
                if (v > Hk->max_abs)
                {
                    Hk->max_abs = v;
                }
            }
            OS_printf("ADCS_SIM: SET_POINTING roll=%d pitch=%d yaw=%d\n", (int)P.roll, (int)P.pitch, (int)P.yaw);
            break;
        default:
            Hk->err_count++;
    }
}

void ADCS_SIM_AppMain(void)
{
    uint32           RunStatus = CFE_ES_RunStatus_APP_RUN;
    CFE_SB_PipeId_t  Pipe;
    CFE_SB_Buffer_t *Buf;
    CFE_Status_t     Status;
    OS_time_t        Now, NextHk;

    CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    CFE_MSG_Init(CFE_MSG_PTR(ADCS_SIM_HkMsg.Hdr), CFE_SB_ValueToMsgId(DEMO_ADCS_HK_TLM_MID), sizeof(ADCS_SIM_HkMsg));
    Status = CFE_SB_CreatePipe(&Pipe, ADCS_SIM_PIPE_DEPTH, "ADCS_SIM_PIPE");
    if (Status == CFE_SUCCESS)
    {
        Status = CFE_SB_SubscribeEx(CFE_SB_ValueToMsgId(DEMO_ADCS_CMD_MID), Pipe, CFE_SB_DEFAULT_QOS, 16);
    }
    if (Status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("ADCS_SIM: cannot start, RC = 0x%08lX\n", (unsigned long)Status);
        RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }
    OS_printf("ADCS_SIM: started\n");
    OS_GetLocalTime(&NextHk);
    NextHk = OS_TimeAdd(NextHk, OS_TimeFromTotalMilliseconds(ADCS_SIM_HK_MS));

    while (CFE_ES_RunLoop(&RunStatus))
    {
        Status = CFE_SB_ReceiveBuffer(&Buf, Pipe, 100);
        if (Status == CFE_SUCCESS)
        {
            ADCS_SIM_Command(Buf);
        }
        else if (Status != CFE_SB_TIME_OUT)
        {
            CFE_ES_WriteToSysLog("ADCS_SIM: pipe read error, RC = 0x%08lX\n", (unsigned long)Status);
            RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
        OS_GetLocalTime(&Now);
        if (OS_TimeGetTotalMilliseconds(OS_TimeSubtract(Now, NextHk)) >= 0)
        {
            NextHk = OS_TimeAdd(NextHk, OS_TimeFromTotalMilliseconds(ADCS_SIM_HK_MS));
            CFE_SB_TimeStampMsg(CFE_MSG_PTR(ADCS_SIM_HkMsg.Hdr));
            CFE_SB_TransmitMsg(CFE_MSG_PTR(ADCS_SIM_HkMsg.Hdr), true);
        }
    }
    CFE_ES_ExitApp(RunStatus);
}
