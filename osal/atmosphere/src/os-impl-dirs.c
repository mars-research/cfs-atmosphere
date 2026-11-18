/*
 * Directories, on the in-memory file system of the runtime.
 */
#include "os-atmosphere.h"
#include "os-impl-dirs.h"
#include "os-shared-dir.h"
#include "os-shared-idmap.h"

OS_impl_dir_internal_record_t OS_impl_dir_table[OS_MAX_NUM_OPEN_DIRS];

int32 OS_Atmosphere_DirAPI_Impl_Init(void)
{
    memset(OS_impl_dir_table, 0, sizeof(OS_impl_dir_table));
    return OS_SUCCESS;
}

int32 OS_DirCreate_Impl(const char *local_path, uint32 access)
{
    int status = art_fs_mkdir(local_path, false);

    (void)access;
    /* As in the other layers, a directory that exists is not an error. */
    if (status == ART_FS_E_EXISTS)
    {
        art_fs_stat_t st;
        if (art_fs_stat(local_path, &st) == 0 && st.is_dir)
        {
            return OS_SUCCESS;
        }
    }
    return OS_Atmosphere_FsStatus(status);
}

int32 OS_DirOpen_Impl(const OS_object_token_t *token, const char *local_path)
{
    OS_impl_dir_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_dir_table, *token);
    art_fs_stat_t                  st;

    if (art_fs_stat(local_path, &st) < 0 || !st.is_dir || strlen(local_path) >= sizeof(impl->path))
    {
        return OS_ERROR;
    }
    strncpy(impl->path, local_path, sizeof(impl->path) - 1);
    impl->path[sizeof(impl->path) - 1] = 0;
    impl->cursor                       = 0;
    return OS_SUCCESS;
}

int32 OS_DirClose_Impl(const OS_object_token_t *token)
{
    OS_impl_dir_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_dir_table, *token);

    impl->path[0] = 0;
    return OS_SUCCESS;
}

int32 OS_DirRead_Impl(const OS_object_token_t *token, os_dirent_t *dirent)
{
    OS_impl_dir_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_dir_table, *token);

    /* The end of the directory is reported as an error, as readdir() does. */
    if (art_fs_readdir(impl->path, &impl->cursor, dirent->FileName, sizeof(dirent->FileName)) != 1)
    {
        return OS_ERROR;
    }
    return OS_SUCCESS;
}

int32 OS_DirRewind_Impl(const OS_object_token_t *token)
{
    OS_impl_dir_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_dir_table, *token);

    impl->cursor = 0;
    return OS_SUCCESS;
}

int32 OS_DirRemove_Impl(const char *local_path)
{
    return OS_Atmosphere_FsStatus(art_fs_rmdir(local_path));
}
