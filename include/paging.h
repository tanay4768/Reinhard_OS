#ifndef REINHARD_PAGING_H
#define REINHARD_PAGING_H

#include <stdbool.h>
#include <stdint.h>

#define PAGE_PRESENT 0x1u
#define PAGE_WRITE   0x2u
#define PAGE_USER    0x4u

void paging_init(void);
bool paging_map(uint32_t virt, uint32_t phys, uint32_t flags);
bool paging_unmap(uint32_t virt, uint32_t *phys_out);
bool paging_query(uint32_t virt, uint32_t *pde, uint32_t *pte);

uint32_t paging_fault_count(void);
uint32_t paging_demand_count(void);

#endif
