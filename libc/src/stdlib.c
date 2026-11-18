#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "atmo_rt.h"

int errno;

static int digit_value(int c)
{
    if (isdigit(c))
        return c - '0';
    if (islower(c))
        return c - 'a' + 10;
    if (isupper(c))
        return c - 'A' + 10;
    return 99;
}

/* Parses an optional sign and the digits. Sets *negative and *overflow. */
static unsigned long long parse(const char *s, char **end, int base, int *negative, int *overflow)
{
    const char *p = s;
    unsigned long long value = 0;
    int any = 0;

    *negative = 0;
    *overflow = 0;
    while (isspace((unsigned char)*p))
        p++;
    if (*p == '+' || *p == '-') {
        *negative = (*p == '-');
        p++;
    }
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X') &&
        digit_value((unsigned char)p[2]) < 16) {
        p += 2;
        base = 16;
    } else if (base == 0) {
        base = (p[0] == '0') ? 8 : 10;
    }
    if (base < 2 || base > 36) {
        if (end != NULL)
            *end = (char *)s;
        errno = EINVAL;
        return 0;
    }
    for (;; p++) {
        int d = digit_value((unsigned char)*p);
        if (d >= base)
            break;
        any = 1;
        if (value > (ULLONG_MAX - (unsigned)d) / (unsigned)base) {
            *overflow = 1;
            value = ULLONG_MAX;
        } else if (!*overflow) {
            value = value * (unsigned)base + (unsigned)d;
        }
    }
    if (end != NULL)
        *end = (char *)(any ? p : s);
    return value;
}

unsigned long long strtoull(const char *s, char **end, int base)
{
    int negative, overflow;
    unsigned long long value = parse(s, end, base, &negative, &overflow);
    if (overflow) {
        errno = ERANGE;
        return ULLONG_MAX;
    }
    return negative ? -value : value;
}

unsigned long strtoul(const char *s, char **end, int base)
{
    return strtoull(s, end, base); /* long and long long have the same width */
}

long long strtoll(const char *s, char **end, int base)
{
    int negative, overflow;
    unsigned long long value = parse(s, end, base, &negative, &overflow);
    unsigned long long limit = negative ? (unsigned long long)LLONG_MAX + 1 : LLONG_MAX;
    if (overflow || value > limit) {
        errno = ERANGE;
        return negative ? LLONG_MIN : LLONG_MAX;
    }
    return negative ? (long long)(-value) : (long long)value;
}

long strtol(const char *s, char **end, int base)
{
    return strtoll(s, end, base);
}

int atoi(const char *s)
{
    return (int)strtol(s, NULL, 10);
}

int abs(int v)
{
    return v < 0 ? -v : v;
}

long labs(long v)
{
    return v < 0 ? -v : v;
}

void exit(int status)
{
    art_exit(status);
}

void abort(void)
{
    fputs("abort() called\n", stderr);
    art_exit(134);
}

void atmo_assert_fail(const char *expr, const char *file, int line)
{
    printf("%s:%d: assertion failed: %s\n", file, line, expr);
    art_exit(134);
}
