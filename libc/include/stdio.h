#ifndef ATMO_LIBC_STDIO_H
#define ATMO_LIBC_STDIO_H
#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)

/* stdout and stderr are the console. fopen() opens a file of the runtime's
 * in-memory file system. */
typedef struct atmo_file FILE;
extern FILE *const stdout;
extern FILE *const stderr;

int printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int vprintf(const char *fmt, va_list ap);
int fprintf(FILE *f, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int vfprintf(FILE *f, const char *fmt, va_list ap);
int snprintf(char *buf, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int sprintf(char *buf, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int puts(const char *s);
int putchar(int c);
int fputs(const char *s, FILE *f);
int fflush(FILE *f);

FILE  *fopen(const char *path, const char *mode);
int    fclose(FILE *f);
size_t fread(void *buf, size_t size, size_t count, FILE *f);
size_t fwrite(const void *buf, size_t size, size_t count, FILE *f);
int    fgetc(FILE *f);

#endif
