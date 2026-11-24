/*
 * TO_TEST: telemetry output without a network.
 *
 * The application subscribes to housekeeping telemetry and prints one line
 * per packet on the console, where TO_LAB would send the packet to a ground
 * system. The housekeeping packets in the list start their payload with the
 * count of accepted commands and the count of rejected commands. The packets
 * of the attack demo get a line with their own fields. Every line ends with
 * the timestamp counter.
 */
#include <string.h>

#include "cfe.h"
#include "cfe_msgids.h"

#include "sample_app_msgids.h"
#include "hk_msgids.h"

#include "atmo_rt.h"
#include "demo_msg.h"

#define TO_TEST_PIPE_DEPTH 48
#define TO_TEST_MSG_LIMIT  24

typedef struct
{
    CFE_SB_MsgId_Atom_t MsgId;
    const char         *Name;
} TO_TEST_Packet_t;

static const TO_TEST_Packet_t TO_TEST_Packets[] = {
    {CFE_ES_HK_TLM_MID, "CFE_ES_HK"},   {CFE_EVS_HK_TLM_MID, "CFE_EVS_HK"},       {CFE_SB_HK_TLM_MID, "CFE_SB_HK"},
    {CFE_TBL_HK_TLM_MID, "CFE_TBL_HK"}, {CFE_TIME_HK_TLM_MID, "CFE_TIME_HK"},     {SAMPLE_APP_HK_TLM_MID, "SAMPLE_APP_HK"},
    {HK_HK_TLM_MID, "HK_HK"},
    {DEMO_IMAGER_HK_TLM_MID, "IMAGER_HK"}, {DEMO_ADCS_HK_TLM_MID, "ADCS_HK"},         {DEMO_BUS_HK_TLM_MID, "BUS_HK"},
    {DEMO_BUS_EVENT_TLM_MID, "BUS_EVENT"}, {DEMO_PAYLOAD_REPORT_MID, "PAYLOAD_REPORT"},
};

#define TO_TEST_NUM_PACKETS (sizeof(TO_TEST_Packets) / sizeof(TO_TEST_Packets[0]))

/* Prints a packet of the attack demo. Returns false for any other packet. */
static bool TO_TEST_Demo(CFE_SB_MsgId_Atom_t MsgId, const uint8 *Payload, size_t Size)
{
    unsigned long Tsc = (unsigned long)art_sys_rdtsc();

    if (MsgId == DEMO_IMAGER_HK_TLM_MID && Size == sizeof(struct demo_imager_hk))
    {
        struct demo_imager_hk Hk;
        memcpy(&Hk, Payload, sizeof(Hk));
        OS_printf("TO_TEST: IMAGER_HK     cmd=%u err=%u counter=%u attacks=%u generation=%u tsc=%lu\n",
                  (unsigned int)Hk.cmd_count, (unsigned int)Hk.err_count, (unsigned int)Hk.counter,
                  (unsigned int)Hk.attacks_seen, (unsigned int)Hk.generation, Tsc);
    }
    else if (MsgId == DEMO_ADCS_HK_TLM_MID && Size == sizeof(struct demo_adcs_hk))
    {
        struct demo_adcs_hk Hk;
        memcpy(&Hk, Payload, sizeof(Hk));
        OS_printf("TO_TEST: ADCS_HK       cmd=%u err=%u roll=%d pitch=%d yaw=%d accepted=%u max_abs=%d tsc=%lu\n",
                  (unsigned int)Hk.cmd_count, (unsigned int)Hk.err_count, (int)Hk.roll, (int)Hk.pitch, (int)Hk.yaw,
                  (unsigned int)Hk.accepted, (int)Hk.max_abs, Tsc);
    }
    else if (MsgId == DEMO_BUS_HK_TLM_MID && Size == sizeof(struct demo_bus_hk))
    {
        struct demo_bus_hk Hk;
        memcpy(&Hk, Payload, sizeof(Hk));
        OS_printf("TO_TEST: BUS_HK        fwd_mc=%u fwd_payload=%u denied_payload=%u dropped_payload=%u "
                  "restart_requests=%u tsc=%lu\n",
                  (unsigned int)Hk.forwarded[DEMO_PROC_MC], (unsigned int)Hk.forwarded[DEMO_PROC_PAYLOAD],
                  (unsigned int)Hk.denied[DEMO_PROC_PAYLOAD], (unsigned int)Hk.dropped[DEMO_PROC_PAYLOAD],
                  (unsigned int)Hk.restart_requests, Tsc);
    }
    else if (MsgId == DEMO_BUS_EVENT_TLM_MID && Size == sizeof(struct demo_bus_event))
    {
        struct demo_bus_event Ev;
        memcpy(&Ev, Payload, sizeof(Ev));
        OS_printf("TO_TEST: BUS_EVENT     sender=%u reason=%u mid=0x%04X cc=%u denials=%u tsc=%lu\n",
                  (unsigned int)Ev.sender, (unsigned int)Ev.reason, (unsigned int)Ev.msg_id,
                  (unsigned int)Ev.fcn_code, (unsigned int)Ev.denials, Tsc);
    }
    else if (MsgId == DEMO_PAYLOAD_REPORT_MID && Size == DEMO_REPORT_TEXT)
    {
        OS_printf("TO_TEST: PAYLOAD_REPORT %.*s\n", DEMO_REPORT_TEXT - 1, (const char *)Payload);
    }
    else
    {
        return false;
    }
    return true;
}

void TO_TEST_AppMain(void)
{
    uint32           RunStatus = CFE_ES_RunStatus_APP_RUN;
    CFE_SB_PipeId_t  Pipe;
    CFE_SB_Buffer_t *Buf;
    CFE_Status_t     Status;
    size_t           i;

    CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);

    Status = CFE_SB_CreatePipe(&Pipe, TO_TEST_PIPE_DEPTH, "TO_TEST_PIPE");
    if (Status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("TO_TEST: cannot create the pipe, RC = 0x%08lX\n", (unsigned long)Status);
        RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }
    for (i = 0; RunStatus == CFE_ES_RunStatus_APP_RUN && i < TO_TEST_NUM_PACKETS; i++)
    {
        /* The bridge publishes what arrived in one poll in a burst; the
         * default limit of four packets of one ID in the pipe is too low. */
        Status = CFE_SB_SubscribeEx(CFE_SB_ValueToMsgId(TO_TEST_Packets[i].MsgId), Pipe, CFE_SB_DEFAULT_QOS,
                                    TO_TEST_MSG_LIMIT);
        if (Status != CFE_SUCCESS)
        {
            CFE_ES_WriteToSysLog("TO_TEST: cannot subscribe to %s, RC = 0x%08lX\n", TO_TEST_Packets[i].Name,
                                 (unsigned long)Status);
        }
    }
    OS_printf("TO_TEST: started, %u packets subscribed\n", (unsigned int)TO_TEST_NUM_PACKETS);

    while (CFE_ES_RunLoop(&RunStatus))
    {
        CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;
        CFE_MSG_Size_t Size  = 0;
        const char    *Name  = "?";
        const uint8   *Payload;

        Status = CFE_SB_ReceiveBuffer(&Buf, Pipe, CFE_SB_PEND_FOREVER);
        if (Status != CFE_SUCCESS)
        {
            CFE_ES_WriteToSysLog("TO_TEST: pipe read error, RC = 0x%08lX\n", (unsigned long)Status);
            RunStatus = CFE_ES_RunStatus_APP_ERROR;
            break;
        }
        CFE_MSG_GetMsgId(&Buf->Msg, &MsgId);
        CFE_MSG_GetSize(&Buf->Msg, &Size);
        for (i = 0; i < TO_TEST_NUM_PACKETS; i++)
        {
            if (CFE_SB_MsgIdToValue(MsgId) == TO_TEST_Packets[i].MsgId)
            {
                Name = TO_TEST_Packets[i].Name;
            }
        }
        Payload = (const uint8 *)Buf + sizeof(CFE_MSG_TelemetryHeader_t);
        if (TO_TEST_Demo(CFE_SB_MsgIdToValue(MsgId), Payload, Size - sizeof(CFE_MSG_TelemetryHeader_t)))
        {
            /* Printed. */
        }
        else if (Size >= sizeof(CFE_MSG_TelemetryHeader_t) + 2)
        {
            OS_printf("TO_TEST: %-13s mid=0x%04X len=%u cmd=%u err=%u tsc=%lu\n", Name,
                      (unsigned int)CFE_SB_MsgIdToValue(MsgId), (unsigned int)Size, (unsigned int)Payload[0],
                      (unsigned int)Payload[1], (unsigned long)art_sys_rdtsc());
        }
        else
        {
            OS_printf("TO_TEST: %-13s mid=0x%04X len=%u\n", Name, (unsigned int)CFE_SB_MsgIdToValue(MsgId),
                      (unsigned int)Size);
        }
    }
    CFE_ES_ExitApp(RunStatus);
}
