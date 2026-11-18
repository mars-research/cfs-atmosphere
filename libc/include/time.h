#ifndef ATMO_LIBC_TIME_H
#define ATMO_LIBC_TIME_H
#include <stddef.h>
#include <stdint.h>

typedef int64_t time_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};

struct tm *gmtime_r(const time_t *t, struct tm *out);
size_t     strftime(char *buf, size_t max, const char *fmt, const struct tm *tm);

#endif
