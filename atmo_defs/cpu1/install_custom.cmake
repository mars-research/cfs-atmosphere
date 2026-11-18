# The SCH_LAB schedule in tables/sch_lab_table.c names messages of these
# applications.
target_include_directories(sch_lab.table INTERFACE
    $<TARGET_PROPERTY:sample_app,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:hk,INCLUDE_DIRECTORIES>
)
