/*
 * The schedule of SCH_LAB for the Atmosphere mission. It replaces the empty
 * default table of the application.
 */
#include "cfe_tbl_filedef.h" /* Required to obtain the CFE_TBL_FILEDEF macro definition */
#include "sch_lab_tbl.h"
#include "cfe_sb_api_typedefs.h" /* Required to use the CFE_SB_MSGID_WRAP_VALUE macro */
#include "cfe_msgids.h"

#include "sample_app_msgids.h"
#include "hk_msgids.h"

/* Entries are {message, period in ticks, function code}. */
SCH_LAB_ScheduleTable_t Schedule = {
    .TickRate = 10, /* ticks per second */
    .Config   = {
        {CFE_SB_MSGID_WRAP_VALUE(SAMPLE_APP_SEND_HK_MID), 10, 0}, /* every 1.0 seconds */
        {CFE_SB_MSGID_WRAP_VALUE(CFE_ES_SEND_HK_MID), 40, 0},     /* every 4.0 seconds */
        {CFE_SB_MSGID_WRAP_VALUE(CFE_EVS_SEND_HK_MID), 42, 0},    /* every 4.2 seconds */
        {CFE_SB_MSGID_WRAP_VALUE(CFE_TIME_SEND_HK_MID), 44, 0},   /* every 4.4 seconds */
        {CFE_SB_MSGID_WRAP_VALUE(CFE_SB_SEND_HK_MID), 46, 0},     /* every 4.6 seconds */
        {CFE_SB_MSGID_WRAP_VALUE(CFE_TBL_SEND_HK_MID), 48, 0},    /* every 4.8 seconds */
        {CFE_SB_MSGID_WRAP_VALUE(HK_SEND_HK_MID), 50, 0},         /* every 5.0 seconds */
    }
};

CFE_TBL_FILEDEF(Schedule, SCH_LAB.Schedule, Schedule Lab MsgID Table, sch_lab_table.tbl)
