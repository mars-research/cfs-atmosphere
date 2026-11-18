/*
 * File system volumes.
 *
 * Every volume is a directory of the in-memory file system of the runtime.
 * The shared layer translates a virtual path to a path below that directory.
 */
#include "os-atmosphere.h"
#include "os-shared-filesys.h"
#include "os-shared-idmap.h"

#define OS_ATMOSPHERE_DEFAULT_BLOCK_SIZE 512
#define OS_ATMOSPHERE_DEFAULT_BLOCKS     65536

int32 OS_Atmosphere_FileSysAPI_Impl_Init(void)
{
    return OS_SUCCESS;
}

int32 OS_FileSysStartVolume_Impl(const OS_object_token_t *token)
{
    OS_filesys_internal_record_t *local = OS_OBJECT_TABLE_GET(OS_filesys_table, *token);

    /* A volume without a given location gets the directory /<volume name>. */
    if (local->system_mountpt[0] == 0 && local->volume_name[0] != 0)
    {
        local->system_mountpt[0] = '/';
        strncpy(&local->system_mountpt[1], local->volume_name, sizeof(local->system_mountpt) - 2);
        local->system_mountpt[sizeof(local->system_mountpt) - 1] = 0;
    }
    if (local->system_mountpt[0] == 0)
    {
        return OS_FS_ERR_DRIVE_NOT_CREATED;
    }
    return OS_Atmosphere_FsStatus(art_fs_mkdir(local->system_mountpt, true));
}

int32 OS_FileSysStopVolume_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_SUCCESS;
}

int32 OS_FileSysFormatVolume_Impl(const OS_object_token_t *token)
{
    OS_filesys_internal_record_t *local = OS_OBJECT_TABLE_GET(OS_filesys_table, *token);

    /* Formatting empties the directory. A fixed mapping of files that came
     * with the image is left alone. */
    if (local->fstype != OS_FILESYS_TYPE_VOLATILE_DISK)
    {
        return OS_SUCCESS;
    }
    return OS_Atmosphere_FsStatus(art_fs_clear(local->system_mountpt));
}

int32 OS_FileSysMountVolume_Impl(const OS_object_token_t *token)
{
    OS_filesys_internal_record_t *local = OS_OBJECT_TABLE_GET(OS_filesys_table, *token);

    return OS_Atmosphere_FsStatus(art_fs_mkdir(local->system_mountpt, true));
}

int32 OS_FileSysUnmountVolume_Impl(const OS_object_token_t *token)
{
    (void)token;
    return OS_SUCCESS;
}

int32 OS_FileSysStatVolume_Impl(const OS_object_token_t *token, OS_statvfs_t *result)
{
    OS_filesys_internal_record_t *local = OS_OBJECT_TABLE_GET(OS_filesys_table, *token);
    uint64_t                      bytes;
    uint32_t                      free_nodes;
    size_t                        block_size;
    osal_blockcount_t             total;
    osal_blockcount_t             used;

    if (art_fs_usage(local->system_mountpt, &bytes, &free_nodes) < 0)
    {
        return OS_ERROR;
    }
    block_size = (local->blocksize != 0) ? local->blocksize : OS_ATMOSPHERE_DEFAULT_BLOCK_SIZE;
    total      = (local->numblocks != 0) ? local->numblocks : OS_ATMOSPHERE_DEFAULT_BLOCKS;
    used       = (osal_blockcount_t)((bytes + block_size - 1) / block_size);

    result->block_size   = block_size;
    result->total_blocks = total;
    result->blocks_free  = (used < total) ? total - used : 0;
    return OS_SUCCESS;
}

int32 OS_FileSysCheckVolume_Impl(const OS_object_token_t *token, bool repair)
{
    (void)token;
    (void)repair;
    return OS_ERR_NOT_IMPLEMENTED;
}
