/* Calendar conversion in UTC. */
#include <stdio.h>
#include <string.h>
#include <time.h>

static int is_leap(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static const int month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

struct tm *gmtime_r(const time_t *t, struct tm *out)
{
    int64_t days = *t / 86400;
    int64_t rem  = *t % 86400;
    int     year = 1970;
    int     month;

    if (rem < 0) {
        rem += 86400;
        days--;
    }
    out->tm_hour = (int)(rem / 3600);
    out->tm_min  = (int)(rem % 3600 / 60);
    out->tm_sec  = (int)(rem % 60);
    out->tm_wday = (int)((days % 7 + 11) % 7); /* 1970-01-01 was a Thursday */

    while (days < 0) {
        year--;
        days += is_leap(year) ? 366 : 365;
    }
    for (;;) {
        int length = is_leap(year) ? 366 : 365;
        if (days < length)
            break;
        days -= length;
        year++;
    }
    out->tm_year = year - 1900;
    out->tm_yday = (int)days;
    for (month = 0; month < 11; month++) {
        int length = month_days[month] + (month == 1 && is_leap(year));
        if (days < length)
            break;
        days -= length;
    }
    out->tm_mon   = month;
    out->tm_mday  = (int)days + 1;
    out->tm_isdst = 0;
    return out;
}

/* The conversions that flight software uses for time stamps. */
size_t strftime(char *buf, size_t max, const char *fmt, const struct tm *tm)
{
    static const char *const months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static const char *const days[7]    = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    size_t                   used       = 0;

    for (; *fmt != '\0'; fmt++) {
        char item[16];
        int  n;

        if (*fmt != '%') {
            item[0] = *fmt;
            item[1] = '\0';
            n       = 1;
        } else {
            fmt++;
            switch (*fmt) {
                case 'Y': n = snprintf(item, sizeof(item), "%d", tm->tm_year + 1900); break;
                case 'y': n = snprintf(item, sizeof(item), "%02d", tm->tm_year % 100); break;
                case 'm': n = snprintf(item, sizeof(item), "%02d", tm->tm_mon + 1); break;
                case 'd': n = snprintf(item, sizeof(item), "%02d", tm->tm_mday); break;
                case 'e': n = snprintf(item, sizeof(item), "%2d", tm->tm_mday); break;
                case 'j': n = snprintf(item, sizeof(item), "%03d", tm->tm_yday + 1); break;
                case 'H': n = snprintf(item, sizeof(item), "%02d", tm->tm_hour); break;
                case 'M': n = snprintf(item, sizeof(item), "%02d", tm->tm_min); break;
                case 'S': n = snprintf(item, sizeof(item), "%02d", tm->tm_sec); break;
                case 'b': n = snprintf(item, sizeof(item), "%s", months[tm->tm_mon % 12]); break;
                case 'a': n = snprintf(item, sizeof(item), "%s", days[tm->tm_wday % 7]); break;
                case '%': n = snprintf(item, sizeof(item), "%%"); break;
                case '\0':
                    fmt--;
                    n = 0;
                    break;
                default: n = snprintf(item, sizeof(item), "%%%c", *fmt); break;
            }
        }
        if (used + (size_t)n >= max)
            return 0;
        memcpy(buf + used, item, (size_t)n);
        used += (size_t)n;
    }
    if (max == 0)
        return 0;
    buf[used] = '\0';
    return used;
}
