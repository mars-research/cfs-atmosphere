#ifndef ATMO_LIBC_INTTYPES_H
#define ATMO_LIBC_INTTYPES_H
#include <stdint.h>

/* x86-64 ELF: int is 32 bits, long and pointers are 64 bits. */
#define PRId8  "d"
#define PRIi8  "i"
#define PRIu8  "u"
#define PRIx8  "x"
#define PRIX8  "X"
#define PRId16 "d"
#define PRIi16 "i"
#define PRIu16 "u"
#define PRIx16 "x"
#define PRIX16 "X"
#define PRId32 "d"
#define PRIi32 "i"
#define PRIu32 "u"
#define PRIx32 "x"
#define PRIX32 "X"
#define PRId64 "ld"
#define PRIi64 "li"
#define PRIu64 "lu"
#define PRIx64 "lx"
#define PRIX64 "lX"
#define PRIdPTR "ld"
#define PRIuPTR "lu"
#define PRIxPTR "lx"
#define PRIXPTR "lX"
#define PRIdMAX "ld"
#define PRIuMAX "lu"
#define PRIxMAX "lx"

#endif
