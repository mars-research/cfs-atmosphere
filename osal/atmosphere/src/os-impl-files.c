/*
 * Files, on the in-memory file system of the runtime.
 */
#include "os-atmosphere.h"
#include "os-impl-files.h"
#include "os-shared-file.h"
#include "os-shared-idmap.h"

int64 OS_Atmosphere_LocalTimeOf(uint64_t monotonic_nsec);

OS_impl_file_internal_record_t OS_impl_filehandle_table[OS_MAX_NUM_OPEN_FILES];

int32 OS_Atmosphere_FsStatus(int64_t result)
{
    if (result >= 0)
    {
        return OS_SUCCESS;
    }
    switch (result)
    {
        case ART_FS_E_NAME_TOO_LONG:
            return OS_FS_ERR_PATH_TOO_LONG;
        case ART_FS_E_INVALID:
            return OS_FS_ERR_PATH_INVALID;
        default:
            return OS_ERROR;
    }
}

int32 OS_Atmosphere_StreamAPI_Impl_Init(void)
{
    osal_index_t i;

    for (i = 0; i < OS_MAX_NUM_OPEN_FILES; i++)
    {
        OS_impl_filehandle_table[i].fd = -1;
    }
    return OS_SUCCESS;
}

int32 OS_FileOpen_Impl(const OS_object_token_t *token, const char *local_path, int32 flags, int32 access_mode)
{
    OS_impl_file_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);
    uint32_t                        mode;
    int                             fd;

    switch (access_mode)
    {
        case OS_READ_ONLY:
            mode = ART_FS_READ;
            break;
        case OS_WRITE_ONLY:
            mode = ART_FS_WRITE;
            break;
        case OS_READ_WRITE:
            mode = ART_FS_READ | ART_FS_WRITE;
            break;
        default:
            return OS_ERROR;
    }
    if (flags & OS_FILE_FLAG_CREATE)
    {
        mode |= ART_FS_CREATE;
    }
    if (flags & OS_FILE_FLAG_TRUNCATE)
    {
        mode |= ART_FS_TRUNCATE;
    }

    fd = art_fs_open(local_path, mode);
    if (fd < 0)
    {
        return OS_Atmosphere_FsStatus(fd);
    }
    impl->fd = fd;
    return OS_SUCCESS;
}

int32 OS_GenericClose_Impl(const OS_object_token_t *token)
{
    OS_impl_file_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);

    art_fs_close(impl->fd);
    impl->fd = -1;
    return OS_SUCCESS;
}

int32 OS_GenericSeek_Impl(const OS_object_token_t *token, osal_offset_t offset, uint32 whence)
{
    OS_impl_file_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);
    uint32_t                        where;
    int64_t                         result;

    switch (whence)
    {
        case OS_SEEK_SET:
            where = 0;
            break;
        case OS_SEEK_CUR:
            where = 1;
            break;
        case OS_SEEK_END:
            where = 2;
            break;
        default:
            return OS_ERROR;
    }
    result = art_fs_seek(impl->fd, (int64_t)offset, where);
    if (result < 0)
    {
        return OS_ERROR;
    }
    /* The API returns the position as a 32-bit status. */
    return (int32)(result & 0x7FFFFFFF);
}

int32 OS_GenericRead_Impl(const OS_object_token_t *token, void *buffer, size_t nbytes, OS_time_t abs_timeout)
{
    OS_impl_file_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);
    int64_t                         result;

    (void)abs_timeout;
    result = art_fs_read(impl->fd, buffer, nbytes);
    return (result < 0) ? OS_ERROR : (int32)result;
}

int32 OS_GenericWrite_Impl(const OS_object_token_t *token, const void *buffer, size_t nbytes, OS_time_t abs_timeout)
{
    OS_impl_file_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);
    int64_t                         result;

    (void)abs_timeout;
    result = art_fs_write(impl->fd, buffer, nbytes);
    return (result < 0) ? OS_ERROR : (int32)result;
}

int32 OS_FileTruncate_Impl(const OS_object_token_t *token, osal_offset_t len)
{
    OS_impl_file_internal_record_t *impl = OS_OBJECT_TABLE_GET(OS_impl_filehandle_table, *token);

    if (len < 0)
    {
        return OS_ERROR;
    }
    return OS_Atmosphere_FsStatus(art_fs_truncate(impl->fd, (size_t)len));
}

int32 OS_FileStat_Impl(const char *local_path, os_fstat_t *filestat)
{
    art_fs_stat_t st;
    int           status = art_fs_stat(local_path, &st);

    if (status < 0)
    {
        return OS_Atmosphere_FsStatus(status);
    }
    filestat->FileSize     = (size_t)st.size;
    filestat->FileTime     = OS_TimeFromTotalNanoseconds(OS_Atmosphere_LocalTimeOf(st.mtime));
    filestat->FileModeBits = OS_FILESTAT_MODE_READ | OS_FILESTAT_MODE_WRITE;
    if (st.is_dir)
    {
        filestat->FileModeBits |= OS_FILESTAT_MODE_DIR | OS_FILESTAT_MODE_EXEC;
    }
    return OS_SUCCESS;
}

int32 OS_FileRemove_Impl(const char *local_path)
{
    return OS_Atmosphere_FsStatus(art_fs_remove(local_path));
}

int32 OS_FileRename_Impl(const char *old_path, const char *new_path)
{
    return OS_Atmosphere_FsStatus(art_fs_rename(old_path, new_path));
}

/* The file system has no access modes. */
int32 OS_FileChmod_Impl(const char *local_path, uint32 access_mode)
{
    art_fs_stat_t st;

    (void)access_mode;
    if (art_fs_stat(local_path, &st) < 0)
    {
        return OS_ERROR;
    }
    return OS_ERR_NOT_IMPLEMENTED;
}
