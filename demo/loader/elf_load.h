/* Loads a statically linked, position-independent ELF64 image for x86-64. */
#ifndef DEMO_ELF_LOAD_H
#define DEMO_ELF_LOAD_H

#include <stddef.h>
#include <stdint.h>

/* Copies the loadable segments of the image in `file` to `base` and applies
 * its relative relocations. The pages [base, base + max_bytes) must be mapped
 * and zero. On success returns 0 and stores the entry address and the number
 * of bytes the image occupies. On failure returns a negative number that
 * names the check that failed. */
int demo_elf_load(const void *file, size_t size, uintptr_t base, size_t max_bytes,
                  uintptr_t *entry, size_t *bytes);

#endif
