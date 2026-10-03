/* Physical memory manager: one bit per 4 KiB frame (1 = used, 0 = free). */

#include "pmm.h"

#include "kernel.h"
#include "kstring.h"

#define MAX_FRAMES (1u << 20)                 /* covers 4 GiB */
#define FRAME_SHIFT 12

extern char kernel_end[];                     /* provided by linker.ld */

static uint32_t bitmap[MAX_FRAMES / 32];
static uint32_t total_frames;                 /* highest usable frame + 1 */
static uint32_t used_frames;
static uint32_t search_hint;

static inline bool frame_used(uint32_t f)  { return bitmap[f / 32] & (1u << (f % 32)); }
static inline void frame_set(uint32_t f)   { bitmap[f / 32] |= (1u << (f % 32)); }
static inline void frame_clear(uint32_t f) { bitmap[f / 32] &= ~(1u << (f % 32)); }

static void free_region(uint64_t base, uint64_t length)
{
    uint64_t start = (base + (PAGE_SIZE - 1)) & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t end   = (base + length) & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t limit = (uint64_t)MAX_FRAMES << FRAME_SHIFT;

    if (end > limit)
        end = limit;

    for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
        uint32_t f = (uint32_t)(addr >> FRAME_SHIFT);
        frame_clear(f);
        if (f >= total_frames)
            total_frames = f + 1;
    }
}

void pmm_init(const multiboot_info_t *mbi)
{
    memset(bitmap, 0xFF, sizeof(bitmap));     /* everything starts reserved */

    if (mbi->flags & MULTIBOOT_FLAG_MMAP) {
        uint32_t addr = mbi->mmap_addr;
        uint32_t end  = mbi->mmap_addr + mbi->mmap_length;

        while (addr < end) {
            const multiboot_mmap_entry_t *e = (const multiboot_mmap_entry_t *)addr;
            if (e->type == MULTIBOOT_MMAP_AVAILABLE)
                free_region(e->addr, e->len);
            addr += e->size + sizeof(e->size);
        }
    } else if (mbi->flags & MULTIBOOT_FLAG_MEM) {
        free_region(0x100000, (uint64_t)mbi->mem_upper * 1024);
    } else {
        PANIC("Bootloader did not provide memory information");
    }

    /* Reserve low memory (BIOS, VGA, ...) and the kernel image itself. */
    uint32_t reserved = ALIGN_UP((uint32_t)kernel_end, PAGE_SIZE) >> FRAME_SHIFT;
    for (uint32_t f = 0; f < reserved && f < MAX_FRAMES; f++)
        frame_set(f);

    used_frames = 0;
    for (uint32_t f = 0; f < total_frames; f++)
        if (frame_used(f))
            used_frames++;
}

uint32_t pmm_alloc_frame(void)
{
    for (uint32_t i = 0; i < total_frames; i++) {
        uint32_t f = (search_hint + i) % total_frames;
        if (!frame_used(f)) {
            frame_set(f);
            used_frames++;
            search_hint = f + 1;
            return f << FRAME_SHIFT;
        }
    }
    return 0;
}

void pmm_free_frame(uint32_t phys)
{
    uint32_t f = phys >> FRAME_SHIFT;
    if (f >= total_frames || !frame_used(f))
        return;                               /* double free / bogus address */

    frame_clear(f);
    used_frames--;
    if (f < search_hint)
        search_hint = f;
}

uint32_t pmm_total_frames(void) { return total_frames; }
uint32_t pmm_used_frames(void)  { return used_frames; }
