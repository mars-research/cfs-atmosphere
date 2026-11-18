/*
 * Message queues.
 */
#include "os-atmosphere.h"
#include "os-impl-queues.h"
#include "os-shared-queue.h"
#include "os-shared-idmap.h"

OS_impl_queue_internal_record_t OS_impl_queue_table[OS_MAX_QUEUES];

int32 OS_Atmosphere_QueueAPI_Impl_Init(void)
{
    memset(OS_impl_queue_table, 0, sizeof(OS_impl_queue_table));
    return OS_SUCCESS;
}

int32 OS_QueueCreate_Impl(const OS_object_token_t *token, uint32 flags)
{
    OS_impl_queue_internal_record_t *impl  = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);
    OS_queue_internal_record_t      *queue = OS_OBJECT_TABLE_GET(OS_queue_table, *token);

    (void)flags;
    if (art_queue_init(&impl->queue, (uint32_t)queue->max_depth, (uint32_t)queue->max_size) != ART_OK)
    {
        return OS_ERROR;
    }
    return OS_SUCCESS;
}

int32 OS_QueueDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_queue_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);

    art_queue_destroy(&impl->queue);
    return OS_SUCCESS;
}

int32 OS_QueueGet_Impl(const OS_object_token_t *token, void *data, size_t size, size_t *size_copied, int32 timeout)
{
    OS_impl_queue_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);
    uint64_t                         deadline;
    size_t                           copied = 0;
    int32                            return_code;

    if (timeout == OS_PEND)
    {
        deadline = ART_FOREVER;
    }
    else if (timeout == OS_CHECK)
    {
        deadline = ART_NO_WAIT;
    }
    else
    {
        deadline = OS_Atmosphere_Deadline((uint32)timeout);
    }

    switch (art_queue_get(&impl->queue, data, size, &copied, deadline))
    {
        case ART_OK:
            return_code = OS_SUCCESS;
            break;
        case ART_EMPTY:
            return_code = OS_QUEUE_EMPTY;
            break;
        case ART_TIMEOUT:
            return_code = OS_QUEUE_TIMEOUT;
            break;
        case ART_TOO_LARGE:
            return_code = OS_QUEUE_INVALID_SIZE;
            break;
        default:
            return_code = OS_ERROR;
            break;
    }
    *size_copied = OSAL_SIZE_C(copied);
    return return_code;
}

int32 OS_QueuePut_Impl(const OS_object_token_t *token, const void *data, size_t size, uint32 flags)
{
    OS_impl_queue_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);

    (void)flags;
    switch (art_queue_put(&impl->queue, data, size))
    {
        case ART_OK:
            return OS_SUCCESS;
        case ART_FULL:
            return OS_QUEUE_FULL;
        case ART_TOO_LARGE:
            return OS_QUEUE_INVALID_SIZE;
        default:
            return OS_ERROR;
    }
}

int32 OS_QueueGetInfo_Impl(const OS_object_token_t *token, OS_queue_prop_t *queue_prop)
{
    (void)token;
    (void)queue_prop;
    return OS_SUCCESS;
}
