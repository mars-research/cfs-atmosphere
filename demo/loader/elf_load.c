/* See elf_load.h. The file is not trusted to be well formed: every offset is
 * checked against the file size and every address against the destination. */
#include "elf_load.h"

#include <string.h>

struct ehdr {
    uint8_t  ident[16];
    uint16_t type, machine;
    uint32_t version;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};
struct phdr {
    uint32_t type, flags;
    uint64_t offset, vaddr, paddr, filesz, memsz, align;
};
struct dyn {
    int64_t  tag;
    uint64_t val;
};
struct rela {
    uint64_t offset, info;
    int64_t  addend;
};

#define PT_LOAD           1
#define PT_DYNAMIC        2
#define ET_DYN            3
#define EM_X86_64         62
#define DT_RELA           7
#define DT_RELASZ         8
#define R_X86_64_NONE     0
#define R_X86_64_RELATIVE 8

static int in_range(uint64_t start, uint64_t len, uint64_t limit)
{
    return start <= limit && len <= limit - start;
}

int demo_elf_load(const void *file, size_t size, uintptr_t base, size_t max_bytes,
                  uintptr_t *entry, size_t *bytes)
{
    const uint8_t *f = file;
    struct ehdr    eh;
    uint64_t       end = 0, dyn_off = 0, dyn_size = 0, rela = 0, rela_size = 0;

    if (size < sizeof(eh))
        return -1;
    memcpy(&eh, f, sizeof(eh));
    if (memcmp(eh.ident, "\177ELF", 4) != 0 || eh.ident[4] != 2 || eh.ident[5] != 1)
        return -2;
    if (eh.type != ET_DYN || eh.machine != EM_X86_64 || eh.phentsize != sizeof(struct phdr))
        return -3;
    if (!in_range(eh.phoff, (uint64_t)eh.phnum * sizeof(struct phdr), size))
        return -4;

    for (unsigned i = 0; i < eh.phnum; i++) {
        struct phdr ph;
        memcpy(&ph, f + eh.phoff + i * sizeof(ph), sizeof(ph));
        if (ph.type == PT_LOAD) {
            if (ph.filesz > ph.memsz || !in_range(ph.offset, ph.filesz, size) ||
                !in_range(ph.vaddr, ph.memsz, max_bytes))
                return -5;
            memcpy((void *)(base + ph.vaddr), f + ph.offset, ph.filesz);
            if (ph.vaddr + ph.memsz > end)
                end = ph.vaddr + ph.memsz;
        } else if (ph.type == PT_DYNAMIC) {
            if (!in_range(ph.offset, ph.filesz, size))
                return -6;
            dyn_off  = ph.offset;
            dyn_size = ph.filesz;
        }
    }
    if (end == 0 || eh.entry >= end)
        return -7;

    for (uint64_t off = 0; off + sizeof(struct dyn) <= dyn_size; off += sizeof(struct dyn)) {
        struct dyn d;
        memcpy(&d, f + dyn_off + off, sizeof(d));
        if (d.tag == DT_RELA)
            rela = d.val;
        else if (d.tag == DT_RELASZ)
            rela_size = d.val;
    }
    /* The relocation table is addressed in the loaded image. */
    if (rela_size != 0 && !in_range(rela, rela_size, end))
        return -8;
    for (uint64_t off = 0; off + sizeof(struct rela) <= rela_size; off += sizeof(struct rela)) {
        struct rela r;
        uint64_t    value;
        memcpy(&r, (const void *)(base + rela + off), sizeof(r));
        if ((uint32_t)r.info == R_X86_64_NONE)
            continue;
        if ((uint32_t)r.info != R_X86_64_RELATIVE || !in_range(r.offset, 8, end))
            return -9;
        value = base + (uint64_t)r.addend;
        memcpy((void *)(base + r.offset), &value, sizeof(value));
    }

    *entry = base + eh.entry;
    *bytes = end;
    return 0;
}
