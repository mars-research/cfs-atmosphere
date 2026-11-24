/*
 * A single-producer, single-consumer ring in two shared pages: a header page
 * and a data page. A record is a 16-bit length followed by that many bytes.
 *
 * `tail` counts the bytes produced and `head` the bytes consumed; both run
 * freely and wrap at 2^32. The data offset of a count is its low 12 bits.
 *
 * The page is writable by both sides, so neither side may trust what the
 * other one controls. The functions here keep their own count privately
 * (`demo_ring_end`) and treat the shared counts of the other side as input
 * that may be anything. The Rust module demo/bus/src/ring.rs has the same
 * layout.
 */
#ifndef DEMO_RING_H
#define DEMO_RING_H

#include <stddef.h>
#include <stdint.h>

#define DEMO_RING_DATA     4096u
#define DEMO_RING_MASK     (DEMO_RING_DATA - 1)
#define DEMO_RING_MAX_REC  1024u   /* the largest record payload */

struct demo_ring {
    /* Written by the producer. */
    volatile uint32_t tail;
    uint32_t          pad0[15];
    /* Written by the consumer. */
    volatile uint32_t head;
    uint32_t          pad1[15];
    /* Written by the process that owns the ring pair: a counter that it
     * increments while it runs, and a word for its state. */
    volatile uint64_t heartbeat;
    volatile uint64_t status;
    /* Words for the two ends' own use; see demo_mc.c and the bus. */
    volatile uint64_t arg[4];
    uint8_t           pad2[4096 - 128 - 48];
    volatile uint8_t  data[DEMO_RING_DATA];
};

_Static_assert(sizeof(struct demo_ring) == 8192, "a ring is two pages");
_Static_assert(offsetof(struct demo_ring, head) == 64, "head");
_Static_assert(offsetof(struct demo_ring, heartbeat) == 128, "heartbeat");
_Static_assert(offsetof(struct demo_ring, arg) == 144, "arg");
_Static_assert(offsetof(struct demo_ring, data) == 4096, "data");

/* One end of a ring, with the count that this side owns. */
struct demo_ring_end {
    struct demo_ring *ring;
    uint32_t          mine;
};

static inline void demo_ring_copy_in(struct demo_ring *r, uint32_t at, const uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        r->data[(at + i) & DEMO_RING_MASK] = src[i];
}

static inline void demo_ring_copy_out(struct demo_ring *r, uint32_t at, uint8_t *dst, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        dst[i] = r->data[(at + i) & DEMO_RING_MASK];
}

/* Producer: appends one record. Returns 0, or -1 if it does not fit now. */
static inline int demo_ring_put(struct demo_ring_end *e, const void *rec, uint32_t len)
{
    struct demo_ring *r = e->ring;
    uint32_t used = e->mine - r->head; /* the consumer's count, as it claims */
    uint8_t  hdr[2] = {(uint8_t)len, (uint8_t)(len >> 8)};

    if (len > DEMO_RING_MAX_REC || used > DEMO_RING_DATA || DEMO_RING_DATA - used < len + 2)
        return -1;
    demo_ring_copy_in(r, e->mine, hdr, 2);
    demo_ring_copy_in(r, e->mine + 2, (const uint8_t *)rec, len);
    e->mine += len + 2;
    r->tail = e->mine;
    return 0;
}

/* Consumer: copies the next record into `buf` (at least DEMO_RING_MAX_REC
 * bytes). Returns its length, 0 if the ring is empty, or -1 if the
 * producer's counts or the record length make no sense; the ring is then
 * emptied, so that the consumer does not see the same garbage again. */
static inline int demo_ring_get(struct demo_ring_end *e, void *buf)
{
    struct demo_ring *r = e->ring;
    uint32_t tail = r->tail; /* read once */
    uint32_t avail = tail - e->mine;
    uint8_t  hdr[2];
    uint32_t len;

    if (avail == 0)
        return 0;
    if (avail > DEMO_RING_DATA || avail < 2)
        goto bad;
    demo_ring_copy_out(r, e->mine, hdr, 2);
    len = hdr[0] | ((uint32_t)hdr[1] << 8);
    if (len == 0 || len > DEMO_RING_MAX_REC || len + 2 > avail)
        goto bad;
    demo_ring_copy_out(r, e->mine + 2, (uint8_t *)buf, len);
    e->mine += len + 2;
    r->head = e->mine;
    return (int)len;
bad:
    e->mine = tail;
    r->head = tail;
    return -1;
}

#endif
