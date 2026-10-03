#ifndef AKIRA_PMM_H
#define AKIRA_PMM_H

#include <stdint.h>
#include "multiboot.h"

void     pmm_init(const multiboot_info_t *mbi);
uint32_t pmm_alloc_frame(void);          /* physical address, or 0 when out of memory */
void     pmm_free_frame(uint32_t phys);

uint32_t pmm_total_frames(void);
uint32_t pmm_used_frames(void);

#endif
