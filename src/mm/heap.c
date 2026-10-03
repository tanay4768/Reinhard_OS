/* Kernel heap: a first-fit free-list allocator.
 *
 * Blocks form a doubly linked list ordered by address; every block is
 * immediately followed by its neighbour, so coalescing is just a pointer
 * check. The heap grows by bumping `brk` inside the reserved virtual range.
 * No physical memory is committed up front - the page-fault handler maps a
 * frame the first time a page is touched (see paging.c).
 */

#include "heap.h"

#include "kernel.h"
#include "kstring.h"

#define BLOCK_MAGIC 0xA11C0000u
#define FLAG_FREE   0x1u
#define MIN_PAYLOAD 8u
#define ALIGNMENT   8u

typedef struct block {
    size_t        size;      /* payload bytes (header excluded) */
    uint32_t      flags;     /* BLOCK_MAGIC | FLAG_FREE?        */
    struct block *prev;
    struct block *next;
} block_t;                   /* 16 bytes on i386 */

#define HEADER_SIZE sizeof(block_t)

static block_t *head, *tail;
static uint32_t brk = KHEAP_START;
static size_t   bytes_used;

static inline bool block_is_free(const block_t *b) { return b->flags & FLAG_FREE; }
static inline void block_set(block_t *b, bool free)
{
    b->flags = BLOCK_MAGIC | (free ? FLAG_FREE : 0);
}

static inline void *payload(block_t *b) { return (uint8_t *)b + HEADER_SIZE; }

/* Reserve more virtual space; the new tail block is merged if tail is free. */
static bool heap_grow(size_t min_payload)
{
    size_t   bytes = ALIGN_UP(min_payload + HEADER_SIZE, PAGE_SIZE);
    uint32_t old   = brk;

    if ((uint64_t)brk + bytes > KHEAP_MAX)
        return false;
    brk += (uint32_t)bytes;

    if (tail && block_is_free(tail)) {
        tail->size += bytes;
        return true;
    }

    block_t *b = (block_t *)old;     /* first touch faults and maps the page */
    b->size = bytes - HEADER_SIZE;
    block_set(b, true);
    b->prev = tail;
    b->next = NULL;
    if (tail)
        tail->next = b;
    else
        head = b;
    tail = b;
    return true;
}

static void split_block(block_t *b, size_t size)
{
    if (b->size < size + HEADER_SIZE + MIN_PAYLOAD)
        return;

    block_t *rest = (block_t *)((uint8_t *)payload(b) + size);
    rest->size = b->size - size - HEADER_SIZE;
    block_set(rest, true);
    rest->prev = b;
    rest->next = b->next;
    if (b->next)
        b->next->prev = rest;
    else
        tail = rest;
    b->next = rest;
    b->size = size;
}

void heap_init(void)
{
    head = tail = NULL;
    brk = KHEAP_START;
    bytes_used = 0;
}

void *kmalloc(size_t size)
{
    if (size == 0)
        return NULL;
    size = ALIGN_UP(size, ALIGNMENT);
    if (size < MIN_PAYLOAD)
        size = MIN_PAYLOAD;

    for (;;) {
        for (block_t *b = head; b; b = b->next) {
            if (block_is_free(b) && b->size >= size) {
                split_block(b, size);
                block_set(b, false);
                bytes_used += b->size;
                return payload(b);
            }
        }
        if (!heap_grow(size))
            return NULL;
    }
}

void *kzalloc(size_t size)
{
    void *p = kmalloc(size);
    if (p)
        memset(p, 0, size);
    return p;
}

void kfree(void *ptr)
{
    if (!ptr)
        return;

    block_t *b = (block_t *)((uint8_t *)ptr - HEADER_SIZE);
    if ((b->flags & ~FLAG_FREE) != BLOCK_MAGIC)
        PANIC("kfree: invalid pointer or heap corruption");
    if (block_is_free(b))
        PANIC("kfree: double free");

    bytes_used -= b->size;
    block_set(b, true);

    if (b->next && block_is_free(b->next)) {          /* merge with next */
        b->size += HEADER_SIZE + b->next->size;
        b->next = b->next->next;
        if (b->next)
            b->next->prev = b;
        else
            tail = b;
    }
    if (b->prev && block_is_free(b->prev)) {          /* merge with previous */
        block_t *p = b->prev;
        p->size += HEADER_SIZE + b->size;
        p->next = b->next;
        if (b->next)
            b->next->prev = p;
        else
            tail = p;
    }
}

void *krealloc(void *ptr, size_t size)
{
    if (!ptr)
        return kmalloc(size);
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    block_t *b = (block_t *)((uint8_t *)ptr - HEADER_SIZE);
    if (b->size >= size)
        return ptr;

    void *fresh = kmalloc(size);
    if (!fresh)
        return NULL;
    memcpy(fresh, ptr, b->size);
    kfree(ptr);
    return fresh;
}

bool heap_contains(uint32_t addr)
{
    return addr >= KHEAP_START && addr < brk;
}

void heap_stats(heap_stats_t *out)
{
    out->mapped = brk - KHEAP_START;
    out->used   = bytes_used;
}
