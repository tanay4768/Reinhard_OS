#ifndef AKIRA_HEAP_H
#define AKIRA_HEAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The kernel heap lives in its own virtual range and is demand-paged:
 * pages are backed by physical frames only when first touched. */
#define KHEAP_START 0xD0000000u
#define KHEAP_MAX   0xD8000000u   /* 128 MiB of address space */

typedef struct {
    size_t mapped;   /* virtual bytes reserved so far */
    size_t used;     /* bytes handed out to callers   */
} heap_stats_t;

void  heap_init(void);
void *kmalloc(size_t size);
void *kzalloc(size_t size);
void *krealloc(void *ptr, size_t size);
void  kfree(void *ptr);

bool  heap_contains(uint32_t addr);
void  heap_stats(heap_stats_t *out);

#endif
