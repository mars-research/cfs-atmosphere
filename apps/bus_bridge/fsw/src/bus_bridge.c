/*
 * BUS_BRIDGE: the gateway between the cFE software bus of the
 * mission-critical process and the bus container.
 *
 * Outbound: it subscribes to the message IDs that are meant for other
 * processes and writes each packet to MC's outbound ring. Inbound: every
 * BUS_BRIDGE_POLL_MS it takes what the bus container forwarded and publishes
 * it on the cFE software bus.
 *
 * The rings belong to SUPERVISOR's part of the process (demo/mc/demo_mc.c).
 */
#include <string.h>

#include "cfe.h"
#include "cfe_msgids.h"

#include "atmo_rt.h"
#include "demo_mc.h"
#include "demo_msg.h"
#include "demo_ring.h"

#define BUS_BRIDGE_PIPE_DEPTH 32
#define BUS_BRIDGE_POLL_MS    10

/* The layouts and numbers of demo_msg.h are those of this cFE build. */
CompileTimeAssert(sizeof(CFE_MSG_CommandHeader_t) == DEMO_CMD_HDR, BUS_BRIDGE_CmdHdr);
CompileTimeAssert(sizeof(CFE_MSG_TelemetryHeader_t) == DEMO_TLM_HDR, BUS_BRIDGE_TlmHdr);
CompileTimeAssert(CFE_PLATFORM_CMD_TOPICID_TO_MIDV(0xD0) == DEMO_PAYLOAD_CMD_MID, BUS_BRIDGE_CmdMid);
CompileTimeAssert(CFE_PLATFORM_TLM_TOPICID_TO_MIDV(0xD0) == DEMO_IMAGER_HK_TLM_MID, BUS_BRIDGE_TlmMid);

/* What MC sends to other processes. */
static const CFE_SB_MsgId_Atom_t BUS_BRIDGE_Outbound[] = {DEMO_PAYLOAD_CMD_MID};

static struct
{
    uint32 Out, OutFull, In, InBad;
} BUS_BRIDGE_Count;

static void BUS_BRIDGE_Inbound(void)
{
    /* Aligned as a software bus buffer, and as large as a ring record. */
    static union
    {
        CFE_SB_Buffer_t Buf;
        uint8           Bytes[DEMO_RING_MAX_REC];
    } Rec;
    int Len;

    while ((Len = demo_mc_recv(Rec.Bytes)) != 0)
    {
        CFE_MSG_Size_t Size = 0;

        /* The bus container checked the packet. Check the one thing the
         * software bus relies on anyway: the length in the header. */
        if (Len < 0 || Len < (int)sizeof(CFE_MSG_CommandHeader_t) ||
            CFE_MSG_GetSize(&Rec.Buf.Msg, &Size) != CFE_SUCCESS || Size != (CFE_MSG_Size_t)Len)
        {
            BUS_BRIDGE_Count.InBad++;
            continue;
        }
        CFE_SB_TransmitMsg(&Rec.Buf.Msg, false);
        BUS_BRIDGE_Count.In++;
    }
}

void BUS_BRIDGE_AppMain(void)
{
    uint32           RunStatus = CFE_ES_RunStatus_APP_RUN;
    CFE_SB_PipeId_t  Pipe;
    CFE_SB_Buffer_t *Buf;
    CFE_Status_t     Status;
    size_t           i;

    CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    Status = CFE_SB_CreatePipe(&Pipe, BUS_BRIDGE_PIPE_DEPTH, "BUS_BRIDGE_PIPE");
    for (i = 0; Status == CFE_SUCCESS && i < sizeof(BUS_BRIDGE_Outbound) / sizeof(BUS_BRIDGE_Outbound[0]); i++)
    {
        Status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(BUS_BRIDGE_Outbound[i]), Pipe);
    }
    if (Status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("BUS_BRIDGE: cannot start, RC = 0x%08lX\n", (unsigned long)Status);
        RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }
    OS_printf("BUS_BRIDGE: started\n");

    while (CFE_ES_RunLoop(&RunStatus))
    {
        Status = CFE_SB_ReceiveBuffer(&Buf, Pipe, BUS_BRIDGE_POLL_MS);
        if (Status == CFE_SUCCESS)
        {
            CFE_MSG_Size_t Size = 0;

            CFE_MSG_GetSize(&Buf->Msg, &Size);
            if (Size <= DEMO_RING_MAX_REC && demo_mc_send(Buf, (uint32)Size) == 0)
            {
                BUS_BRIDGE_Count.Out++;
            }
            else
            {
                BUS_BRIDGE_Count.OutFull++;
            }
        }
        else if (Status != CFE_SB_TIME_OUT)
        {
            CFE_ES_WriteToSysLog("BUS_BRIDGE: pipe read error, RC = 0x%08lX\n", (unsigned long)Status);
            RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
        demo_mc_beat();
        BUS_BRIDGE_Inbound();
    }
    CFE_ES_ExitApp(RunStatus);
}
