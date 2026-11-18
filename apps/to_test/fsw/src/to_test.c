/*
 * TO_TEST: telemetry output without a network.
 *
 * The application subscribes to housekeeping telemetry and prints one line
 * per packet on the console, where TO_LAB would send the packet to a ground
 * system. All the packets in the list start their payload with the count of
 * accepted commands and the count of rejected commands.
 */
#include "cfe.h"
#include "cfe_msgids.h"

#include "sample_app_msgids.h"
#include "hk_msgids.h"

#define TO_TEST_PIPE_DEPTH 32

typedef struct
{
    CFE_SB_MsgId_Atom_t MsgId;
    const char         *Name;
} TO_TEST_Packet_t;

static const TO_TEST_Packet_t TO_TEST_Packets[] = {
    {CFE_ES_HK_TLM_MID, "CFE_ES_HK"},   {CFE_EVS_HK_TLM_MID, "CFE_EVS_HK"},       {CFE_SB_HK_TLM_MID, "CFE_SB_HK"},
    {CFE_TBL_HK_TLM_MID, "CFE_TBL_HK"}, {CFE_TIME_HK_TLM_MID, "CFE_TIME_HK"},     {SAMPLE_APP_HK_TLM_MID, "SAMPLE_APP_HK"},
    {HK_HK_TLM_MID, "HK_HK"},
};

#define TO_TEST_NUM_PACKETS (sizeof(TO_TEST_Packets) / sizeof(TO_TEST_Packets[0]))

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
        Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(TO_TEST_Packets[i].MsgId), Pipe);
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
        if (Size >= sizeof(CFE_MSG_TelemetryHeader_t) + 2)
        {
            Payload = (const uint8 *)Buf + sizeof(CFE_MSG_TelemetryHeader_t);
            OS_printf("TO_TEST: %-13s mid=0x%04X len=%u cmd=%u err=%u\n", Name,
                      (unsigned int)CFE_SB_MsgIdToValue(MsgId), (unsigned int)Size, (unsigned int)Payload[0],
                      (unsigned int)Payload[1]);
        }
        else
        {
            OS_printf("TO_TEST: %-13s mid=0x%04X len=%u\n", Name, (unsigned int)CFE_SB_MsgIdToValue(MsgId),
                      (unsigned int)Size);
        }
    }
    CFE_ES_ExitApp(RunStatus);
}
