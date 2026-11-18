/*
 * The runtime has no heap.
 */
#include "os-atmosphere.h"
#include "os-shared-heap.h"

int32 OS_HeapGetInfo_Impl(OS_heap_prop_t *heap_prop)
{
    (void)heap_prop;
    return OS_ERR_NOT_IMPLEMENTED;
}
