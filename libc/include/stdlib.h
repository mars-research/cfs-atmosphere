#ifndef ATMO_LIBC_STDLIB_H
#define ATMO_LIBC_STDLIB_H
#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

unsigned long      strtoul(const char *s, char **end, int base);
unsigned long long strtoull(const char *s, char **end, int base);
long               strtol(const char *s, char **end, int base);
long long          strtoll(const char *s, char **end, int base);
int                atoi(const char *s);
int                abs(int v);
long               labs(long v);

void exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));

/* A small heap for test programs. Flight code does not use it. */
void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void  free(void *p);

#endif
