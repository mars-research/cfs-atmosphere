#ifndef ATMO_LIBC_ASSERT_H
#define ATMO_LIBC_ASSERT_H

void atmo_assert_fail(const char *expr, const char *file, int line) __attribute__((noreturn));

#ifdef NDEBUG
#define assert(e) ((void)0)
#else
#define assert(e) ((e) ? (void)0 : atmo_assert_fail(#e, __FILE__, __LINE__))
#endif

#endif
