/*
 * CI_TEST: command ingest without a network.
 *
 * The application sends the commands of a built-in script to the software
 * bus, as a ground system would through CI_LAB. With CI_TEST_SHUTDOWN_AT_END
 * it ends the run after the script, so that a test can check the console
 * output.
 *
 * The second half of the script drives the attack demo: each ATTACK command
 * tells the payload container to misbehave in one way (demo_msg.h).
 */
#include <string.h>

#include "cfe.h"
#include "cfe_msgids.h"
#include "cfe_es_fcncodes.h"
#include "cfe_es_msg.h"

#include "sample_app_msgids.h"
#include "sample_app_fcncodes.h"

#include "atmo_rt.h"
#include "demo_msg.h"

#ifndef CI_TEST_SHUTDOWN_AT_END
#define CI_TEST_SHUTDOWN_AT_END 1
#endif

/* A function code that SAMPLE_APP does not have. */
#define CI_TEST_INVALID_CC 99

typedef struct
{
    uint32              DelayMs; /* wait before the command */
    CFE_SB_MsgId_Atom_t MsgId;
    CFE_MSG_FcnCode_t   FcnCode;
    const char         *Text;
    /* For commands of the executive that name an application; else NULL. */
    const char         *AppName;
    /* For DEMO_PAYLOAD_ATTACK_CC: which attack; else 0. */
    uint32              Attack;
} CI_TEST_Step_t;

/* Something in the memory of this process for the payload to try to read. */
static volatile uint64 CI_TEST_Secret = 0x5ec4e7;

static const CI_TEST_Step_t CI_TEST_Script[] = {
    {3000, SAMPLE_APP_CMD_MID, SAMPLE_APP_NOOP_CC, "SAMPLE_APP no-op", NULL},
    {2000, CFE_ES_CMD_MID, CFE_ES_NOOP_CC, "CFE_ES no-op", NULL},
    {2000, SAMPLE_APP_CMD_MID, SAMPLE_APP_NOOP_CC, "SAMPLE_APP no-op", NULL},
    /* SAMPLE_APP answers this with an error event; it does not count it. */
    {2000, SAMPLE_APP_CMD_MID, CI_TEST_INVALID_CC, "SAMPLE_APP invalid function code", NULL},
    /* The executive deletes the task of the application and starts it again. */
    {2000, CFE_ES_CMD_MID, CFE_ES_RESTART_APP_CC, "CFE_ES restart SAMPLE_APP", "SAMPLE_APP"},
    /* The restart takes about five seconds. */
    {8000, SAMPLE_APP_CMD_MID, SAMPLE_APP_NOOP_CC, "SAMPLE_APP no-op", NULL},

    /* The attack demo. Attacks that end with a restart of the payload get
     * more time. */
    {2000, DEMO_PAYLOAD_CMD_MID, DEMO_PAYLOAD_ATTACK_CC, "ATTACK B3 pointing out of range", NULL, DEMO_ATTACK_RANGE},
    {2500, DEMO_PAYLOAD_CMD_MID, DEMO_PAYLOAD_ATTACK_CC, "ATTACK B1 forbidden commands", NULL, DEMO_ATTACK_FORBIDDEN},
    {2500, DEMO_PAYLOAD_CMD_MID, DEMO_PAYLOAD_ATTACK_CC, "ATTACK B2 spoofed sender", NULL, DEMO_ATTACK_SPOOF},
    {2500, DEMO_PAYLOAD_CMD_MID, DEMO_PAYLOAD_ATTACK_CC, "ATTACK K2 around the bus", NULL, DEMO_ATTACK_BYPASS},
    {2500, DEMO_PAYLOAD_CMD_MID, DEMO_PAYLOAD_ATTACK_CC, "ATTACK K3 exhaust memory", NULL, DEMO_ATTACK_EXHAUST},
    {3000, DEMO_PAYLOAD_CMD_MID, DEMO_PAYLOAD_ATTACK_CC, "ATTACK K1 read MC memory", NULL, DEMO_ATTACK_READ_MC},
    {4000, DEMO_PAYLOAD_CMD_MID, DEMO_PAYLOAD_ATTACK_CC, "ATTACK K1 write MC ring", NULL, DEMO_ATTACK_WRITE_RING},
    {4000, DEMO_PAYLOAD_CMD_MID, DEMO_PAYLOAD_ATTACK_CC, "ATTACK B4 malformed packets", NULL, DEMO_ATTACK_MALFORMED},
    {4000, DEMO_PAYLOAD_CMD_MID, DEMO_PAYLOAD_ATTACK_CC, "ATTACK B5 flood", NULL, DEMO_ATTACK_FLOOD},
    {5000, DEMO_PAYLOAD_CMD_MID, DEMO_PAYLOAD_ATTACK_CC, "ATTACK B6 change after check", NULL, DEMO_ATTACK_TOCTOU},
};

/* Time for the last telemetry to come out before the run ends. */
#define CI_TEST_FINAL_DELAY_MS 3000

static void CI_TEST_SendAttack(const CI_TEST_Step_t *Step)
{
    struct
    {
        CFE_MSG_CommandHeader_t Hdr;
        struct demo_attack_cmd  Payload;
    } Cmd;
    CFE_Status_t Status;

    memset(&Cmd, 0, sizeof(Cmd));
    Status = CFE_MSG_Init(CFE_MSG_PTR(Cmd.Hdr), CFE_SB_ValueToMsgId(Step->MsgId), sizeof(Cmd));
    if (Status == CFE_SUCCESS)
    {
        Status = CFE_MSG_SetFcnCode(CFE_MSG_PTR(Cmd.Hdr), Step->FcnCode);
    }
    Cmd.Payload.attack = Step->Attack;
    Cmd.Payload.arg    = (uint64)(cpuaddr)&CI_TEST_Secret;
    /* Before the command goes out: the payload reacts on another processor
     * and its lines must come after this one in the log. */
    OS_printf("CI_TEST: sending %s (attack=%u) tsc=%lu\n", Step->Text, (unsigned int)Step->Attack,
              (unsigned long)art_sys_rdtsc());
    if (Status == CFE_SUCCESS)
    {
        Status = CFE_SB_TransmitMsg(CFE_MSG_PTR(Cmd.Hdr), true);
    }
    if (Status != CFE_SUCCESS)
    {
        OS_printf("CI_TEST: %s was not sent, status=0x%08lX\n", Step->Text, (unsigned long)Status);
    }
}

static void CI_TEST_Send(const CI_TEST_Step_t *Step)
{
    /* The largest command of the script: a header and an application name. */
    CFE_ES_RestartAppCmd_t Cmd;
    size_t                 Size = sizeof(Cmd.CommandHeader);
    CFE_Status_t           Status;

    if (Step->Attack != 0)
    {
        CI_TEST_SendAttack(Step);
        return;
    }
    if (Step->AppName != NULL)
    {
        Size = sizeof(Cmd);
    }
    /* CFE_MSG_Init clears the whole message, so the payload is set after it. */
    memset(&Cmd, 0, sizeof(Cmd));
    Status = CFE_MSG_Init(CFE_MSG_PTR(Cmd.CommandHeader), CFE_SB_ValueToMsgId(Step->MsgId), Size);
    if (Status == CFE_SUCCESS)
    {
        Status = CFE_MSG_SetFcnCode(CFE_MSG_PTR(Cmd.CommandHeader), Step->FcnCode);
    }
    if (Step->AppName != NULL)
    {
        strncpy(Cmd.Payload.Application, Step->AppName, sizeof(Cmd.Payload.Application) - 1);
    }
    if (Status == CFE_SUCCESS)
    {
        Status = CFE_SB_TransmitMsg(CFE_MSG_PTR(Cmd.CommandHeader), true);
    }
    OS_printf("CI_TEST: sent %s (mid=0x%04X cc=%u) status=0x%08lX\n", Step->Text, (unsigned int)Step->MsgId,
              (unsigned int)Step->FcnCode, (unsigned long)Status);
}

void CI_TEST_AppMain(void)
{
    uint32 RunStatus = CFE_ES_RunStatus_APP_RUN;
    size_t i;

    CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    CFE_ES_WaitForSystemState(CFE_ES_SystemState_OPERATIONAL, 30000);
    OS_printf("CI_TEST: started, %u commands in the script, cFE is OPERATIONAL, tsc=%lu\n",
              (unsigned int)(sizeof(CI_TEST_Script) / sizeof(CI_TEST_Script[0])), (unsigned long)art_sys_rdtsc());
    OS_printf("CI_TEST: a word of MC memory is at %p, value 0x%lx\n", (void *)&CI_TEST_Secret,
              (unsigned long)CI_TEST_Secret);

    for (i = 0; i < sizeof(CI_TEST_Script) / sizeof(CI_TEST_Script[0]); i++)
    {
        OS_TaskDelay(CI_TEST_Script[i].DelayMs);
        CI_TEST_Send(&CI_TEST_Script[i]);
    }

    OS_TaskDelay(CI_TEST_FINAL_DELAY_MS);
    OS_printf("CI_TEST: script done, the word of MC memory is 0x%lx\n", (unsigned long)CI_TEST_Secret);
    if (CI_TEST_SHUTDOWN_AT_END)
    {
        OS_ApplicationShutdown(true);
    }

    while (CFE_ES_RunLoop(&RunStatus))
    {
        OS_TaskDelay(1000);
    }
    CFE_ES_ExitApp(RunStatus);
}
