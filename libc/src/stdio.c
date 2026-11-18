/* Formatted output without floating point. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "atmo_rt.h"

struct atmo_file {
    int fd; /* a handle of the runtime's file system, or -1 for the console */
    int used;
};
static struct atmo_file console_stream = {-1, 1};
FILE *const stdout = &console_stream;
FILE *const stderr = &console_stream;

/* Where formatted characters go: a bounded buffer, or the console. */
struct sink {
    char  *buf;   /* NULL for the console */
    size_t size;  /* capacity of buf, including the terminator */
    size_t count; /* characters produced so far */
    char   chunk[128];
    size_t chunk_len;
};

static void sink_flush(struct sink *s)
{
    if (s->buf == NULL && s->chunk_len != 0) {
        art_console_write(s->chunk, s->chunk_len);
        s->chunk_len = 0;
    }
}

static void emit(struct sink *s, char c)
{
    if (s->buf != NULL) {
        if (s->count + 1 < s->size)
            s->buf[s->count] = c;
    } else {
        s->chunk[s->chunk_len++] = c;
        if (s->chunk_len == sizeof(s->chunk))
            sink_flush(s);
    }
    s->count++;
}

static void pad(struct sink *s, char c, int n)
{
    for (; n > 0; n--)
        emit(s, c);
}

struct spec {
    int left, plus, space, alt, zero;
    int width;
    int precision; /* -1 if not given */
};

/* Prints an unsigned value with an optional sign or prefix. */
static void number(struct sink *s, const struct spec *sp, unsigned long long value, unsigned base,
                   int upper, const char *prefix)
{
    char        digits[24];
    int         n = 0;
    const char *set = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int         prefix_len = (int)strlen(prefix);
    int         zeros, total;

    while (value != 0) {
        digits[n++] = set[value % base];
        value /= base;
    }
    /* A zero value prints one digit unless the precision is zero. */
    zeros = (sp->precision >= 0 ? sp->precision : 1) - n;
    if (zeros < 0)
        zeros = 0;
    if (base == 8 && sp->alt && zeros == 0 && n > 0)
        zeros = 1;
    total = prefix_len + zeros + n;
    if (sp->precision < 0 && sp->zero && !sp->left && sp->width > total) {
        zeros += sp->width - total;
        total = sp->width;
    }
    if (!sp->left)
        pad(s, ' ', sp->width - total);
    while (*prefix != '\0')
        emit(s, *prefix++);
    pad(s, '0', zeros);
    while (n > 0)
        emit(s, digits[--n]);
    if (sp->left)
        pad(s, ' ', sp->width - total);
}

static void text(struct sink *s, const struct spec *sp, const char *str, size_t len)
{
    if (!sp->left)
        pad(s, ' ', sp->width - (int)len);
    for (size_t i = 0; i < len; i++)
        emit(s, str[i]);
    if (sp->left)
        pad(s, ' ', sp->width - (int)len);
}

static void format(struct sink *s, const char *fmt, va_list ap)
{
    for (; *fmt != '\0'; fmt++) {
        struct spec sp = {0, 0, 0, 0, 0, 0, -1};
        int         length = 0; /* -2 hh, -1 h, 0 none, 1 l, 2 ll/j/z/t */

        if (*fmt != '%') {
            emit(s, *fmt);
            continue;
        }
        fmt++;
        for (;; fmt++) {
            if (*fmt == '-')
                sp.left = 1;
            else if (*fmt == '+')
                sp.plus = 1;
            else if (*fmt == ' ')
                sp.space = 1;
            else if (*fmt == '#')
                sp.alt = 1;
            else if (*fmt == '0')
                sp.zero = 1;
            else
                break;
        }
        if (*fmt == '*') {
            sp.width = va_arg(ap, int);
            if (sp.width < 0) {
                sp.left = 1;
                sp.width = -sp.width;
            }
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9')
                sp.width = sp.width * 10 + (*fmt++ - '0');
        }
        if (*fmt == '.') {
            fmt++;
            sp.precision = 0;
            if (*fmt == '*') {
                sp.precision = va_arg(ap, int);
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9')
                    sp.precision = sp.precision * 10 + (*fmt++ - '0');
            }
        }
        for (;; fmt++) {
            if (*fmt == 'h')
                length = (length == -1) ? -2 : -1;
            else if (*fmt == 'l')
                length = (length == 1) ? 2 : 1;
            else if (*fmt == 'j' || *fmt == 'z' || *fmt == 't')
                length = 2;
            else
                break;
        }

        switch (*fmt) {
            case 'd':
            case 'i': {
                long long v;
                if (length >= 1)
                    v = (length == 1) ? va_arg(ap, long) : va_arg(ap, long long);
                else
                    v = va_arg(ap, int);
                if (length == -1)
                    v = (short)v;
                else if (length == -2)
                    v = (signed char)v;
                number(s, &sp, v < 0 ? -(unsigned long long)v : (unsigned long long)v, 10, 0,
                       v < 0 ? "-" : sp.plus ? "+" : sp.space ? " " : "");
                break;
            }
            case 'u':
            case 'x':
            case 'X':
            case 'o': {
                unsigned long long v;
                unsigned           base = (*fmt == 'u') ? 10 : (*fmt == 'o') ? 8 : 16;
                if (length >= 1)
                    v = (length == 1) ? va_arg(ap, unsigned long) : va_arg(ap, unsigned long long);
                else
                    v = va_arg(ap, unsigned int);
                if (length == -1)
                    v = (unsigned short)v;
                else if (length == -2)
                    v = (unsigned char)v;
                number(s, &sp, v, base, *fmt == 'X',
                       (sp.alt && base == 16 && v != 0) ? (*fmt == 'X' ? "0X" : "0x") : "");
                break;
            }
            case 'p': {
                uintptr_t v = (uintptr_t)va_arg(ap, void *);
                number(s, &sp, v, 16, 0, "0x");
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                text(s, &sp, &c, 1);
                break;
            }
            case 's': {
                const char *str = va_arg(ap, const char *);
                size_t      len;
                if (str == NULL)
                    str = "(null)";
                len = (sp.precision >= 0) ? strnlen(str, (size_t)sp.precision) : strlen(str);
                text(s, &sp, str, len);
                break;
            }
            case '%':
                emit(s, '%');
                break;
            case '\0':
                fmt--; /* a lone '%' at the end */
                break;
            default:
                /* Unknown conversion, floating point included: print it as written. */
                emit(s, '%');
                emit(s, *fmt);
                break;
        }
    }
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    struct sink s = {.buf = buf, .size = size};
    char        none;

    if (buf == NULL || size == 0) {
        s.buf  = &none;
        s.size = 0;
    }
    format(&s, fmt, ap);
    if (size != 0 && buf != NULL)
        buf[s.count < size ? s.count : size - 1] = '\0';
    return (int)s.count;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    int     n;
    va_start(ap, fmt);
    n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int     n;
    va_start(ap, fmt);
    n = vsnprintf(buf, (size_t)1 << 30, fmt, ap);
    va_end(ap);
    return n;
}

int vprintf(const char *fmt, va_list ap)
{
    struct sink s = {.buf = NULL};
    format(&s, fmt, ap);
    sink_flush(&s);
    return (int)s.count;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    int     n;
    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

int vfprintf(FILE *f, const char *fmt, va_list ap)
{
    (void)f;
    return vprintf(fmt, ap);
}

int fprintf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    int     n;
    (void)f;
    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

int fputs(const char *str, FILE *f)
{
    (void)f;
    art_console_write(str, strlen(str));
    return 0;
}

int puts(const char *str)
{
    art_console_write(str, strlen(str));
    art_console_write("\n", 1);
    return 0;
}

int putchar(int c)
{
    char ch = (char)c;
    art_console_write(&ch, 1);
    return (unsigned char)ch;
}

int fflush(FILE *f)
{
    (void)f;
    art_console_flush();
    return 0;
}

/* ---- Files ---- */

#define MAX_STREAMS 8
static struct atmo_file streams[MAX_STREAMS];

FILE *fopen(const char *path, const char *mode)
{
    uint32_t flags;
    int      fd;

    if (mode[0] == 'r')
        flags = ART_FS_READ;
    else if (mode[0] == 'w')
        flags = ART_FS_WRITE | ART_FS_CREATE | ART_FS_TRUNCATE;
    else
        return NULL;
    if (mode[1] == '+' || (mode[1] != '\0' && mode[2] == '+'))
        flags |= ART_FS_READ | ART_FS_WRITE;

    for (int i = 0; i < MAX_STREAMS; i++) {
        if (!streams[i].used) {
            fd = art_fs_open(path, flags);
            if (fd < 0)
                return NULL;
            streams[i].fd   = fd;
            streams[i].used = 1;
            return &streams[i];
        }
    }
    return NULL;
}

int fclose(FILE *f)
{
    if (f == NULL || f->fd < 0)
        return EOF;
    art_fs_close(f->fd);
    f->used = 0;
    return 0;
}

size_t fread(void *buf, size_t size, size_t count, FILE *f)
{
    int64_t n;
    if (f->fd < 0 || size == 0)
        return 0;
    n = art_fs_read(f->fd, buf, size * count);
    return n < 0 ? 0 : (size_t)n / size;
}

size_t fwrite(const void *buf, size_t size, size_t count, FILE *f)
{
    int64_t n;
    if (size == 0)
        return 0;
    if (f->fd < 0) {
        art_console_write(buf, size * count);
        return count;
    }
    n = art_fs_write(f->fd, buf, size * count);
    return n < 0 ? 0 : (size_t)n / size;
}

int fgetc(FILE *f)
{
    unsigned char c;
    if (f->fd < 0 || art_fs_read(f->fd, &c, 1) != 1)
        return EOF;
    return c;
}
