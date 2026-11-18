/* A first-fit heap for test programs. Flight code does not allocate.
 *
 * Blocks are kept in one list ordered by address, so that neighbors merge
 * when they are freed. Memory comes from the runtime in chunks of pages and
 * is never returned to it. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "atmo_rt.h"

struct block {
    size_t        size; /* bytes after the header */
    struct block *next; /* next free block, by address */
};

#define ALIGNMENT   16u
#define CHUNK_PAGES 64u

static struct block *free_list;

static size_t round_up(size_t n)
{
    return (n + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1);
}

void free(void *p)
{
    struct block *b, *prev = NULL, *at;

    if (p == NULL)
        return;
    b = (struct block *)p - 1;
    for (at = free_list; at != NULL && at < b; at = at->next)
        prev = at;
    b->next = at;
    if (prev == NULL)
        free_list = b;
    else
        prev->next = b;
    /* Merge with the block after, then with the block before. */
    if (at != NULL && (char *)(b + 1) + b->size == (char *)at) {
        b->size += sizeof(struct block) + at->size;
        b->next = at->next;
    }
    if (prev != NULL && (char *)(prev + 1) + prev->size == (char *)b) {
        prev->size += sizeof(struct block) + b->size;
        prev->next = b->next;
    }
}

void *malloc(size_t size)
{
    struct block *b, *prev;
    int           grown = 0;

    if (size == 0)
        size = 1;
    size = round_up(size);
    for (;;) {
        prev = NULL;
        for (b = free_list; b != NULL; prev = b, b = b->next) {
            if (b->size < size)
                continue;
            if (b->size >= size + sizeof(struct block) + ALIGNMENT) {
                /* Split: the tail stays free. */
                struct block *rest = (struct block *)((char *)(b + 1) + size);
                rest->size         = b->size - size - sizeof(struct block);
                rest->next         = b->next;
                b->size            = size;
                b->next            = rest;
            }
            if (prev == NULL)
                free_list = b->next;
            else
                prev->next = b->next;
            return b + 1;
        }
        if (grown)
            return NULL;
        {
            size_t pages = (size + sizeof(struct block) + ART_PAGE_SIZE - 1) / ART_PAGE_SIZE;
            if (pages < CHUNK_PAGES)
                pages = CHUNK_PAGES;
            b = art_pages_alloc(pages);
            if (b == NULL)
                return NULL;
            b->size = pages * ART_PAGE_SIZE - sizeof(struct block);
            free(b + 1);
            grown = 1;
        }
    }
}

void *calloc(size_t count, size_t size)
{
    void *p;
    if (size != 0 && count > SIZE_MAX / size)
        return NULL;
    p = malloc(count * size);
    if (p != NULL)
        memset(p, 0, count * size);
    return p;
}
