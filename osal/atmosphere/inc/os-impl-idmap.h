#ifndef OS_IMPL_IDMAP_H
#define OS_IMPL_IDMAP_H

#include "osconfig.h"
#include "osapi-idmap.h"
#include "atmo_rt.h"

typedef struct
{
    art_mutex_t mutex;
} OS_impl_objtype_lock_t;

extern OS_impl_objtype_lock_t *const OS_impl_objtype_lock_table[OS_OBJECT_TYPE_USER];

#endif /* OS_IMPL_IDMAP_H */
