######################################################################
#
# Mission definitions: cFS on Atmosphere, one processor
#
# All applications are linked into the cFE executable, because Atmosphere has
# no dynamic loader. Each entry of cpu1_STATIC_SYMLIST is
# "entry point,cFE name"; the cFE name must be the fourth field of the
# application's line in cpu1_cfe_es_startup.scr.
#
######################################################################

SET(MISSION_NAME "AtmosphereMission")
SET(SPACECRAFT_ID 0x42)

# No dynamically loaded applications. This also removes the test
# applications that the cFE adds by default.
SET(MISSION_GLOBAL_APPLIST)

SET(MISSION_CPUNAMES cpu1)

SET(cpu1_PROCESSORID 1)
SET(cpu1_SYSTEM x86_64-atmosphere)
SET(cpu1_APPLIST)
SET(cpu1_STATIC_APPLIST sample_lib sample_app sch_lab hk ci_test to_test)
SET(cpu1_STATIC_SYMLIST
    SAMPLE_LIB_Init,SAMPLE_LIB
    SAMPLE_APP_Main,SAMPLE_APP
    SCH_LAB_AppMain,SCH_LAB
    HK_AppMain,HK
    CI_TEST_AppMain,CI_TEST
    TO_TEST_AppMain,TO_TEST
)
SET(cpu1_FILELIST cfe_es_startup.scr)
