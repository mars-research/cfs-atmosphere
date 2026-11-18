#ifndef ATMO_LIBC_ERRNO_H
#define ATMO_LIBC_ERRNO_H

extern int errno;

#define EPERM   1
#define ENOENT  2
#define EIO     5
#define EBADF   9
#define EAGAIN  11
#define ENOMEM  12
#define EACCES  13
#define EEXIST  17
#define EINVAL  22
#define ENOSPC  28
#define ERANGE  34
#define ENOSYS  38

#endif
