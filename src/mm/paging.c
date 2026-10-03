/* Two-level x86 paging.
 *
 *  - The first 8 MiB are identity-mapped (kernel image, VGA buffer), except
 *    page 0, which is left unmapped so NULL dereferences fault.
 *  - The last page-directory entry points back at the directory itself
 *    ("recursive mapping"). That makes every page table reachable at a fixed
 *    virtual address, so we can edit mappings for any physical frame without
 *    having it identity-mapped:
 *        page directory  -> 0xFFFFF000
 *        page table N    -> 0xFFC00000 + N * 4096
 *  - The kernel heap region is demand-paged by the page-fault handler.
 */

#include "paging.h"

#include "heap.h"
#include "io.h"
#include "isr.h"
#include "kernel.h"
#include "kprintf.h"
#include "kstring.h"
#include "pmm.h"
#include "vga.h"

#define ENTRIES_PER_TABLE 1024u
#define RECURSIVE_INDEX   1023u
#define PD_VIRT           ((uint32_t *)0xFFFFF000u)
#define PT_VIRT(pdi)      ((uint32_t *)(0xFFC00000u + ((pdi) << 12)))
#define IDENTITY_MAP_SIZE (8u * 1024u * 1024u)

#define CR0_WRITE_PROTECT 0x00010000u
#define CR0_PAGING        0x80000000u

#define PF_PRESENT 0x1u
#define PF_WRITE   0x2u
#define PF_USER    0x4u
#define PF_RESERVED 0x8u
#define PF_FETCH   0x10u

static uint32_t fault_count;
static uint32_t demand_count;

static inline uint32_t pd_index(uint32_t virt) { return virt >> 22; }
static inline uint32_t pt_index(uint32_t virt) { return (virt >> 12) & 0x3FFu; }

bool paging_map(uint32_t virt, uint32_t phys, uint32_t flags)
{
    uint32_t pdi = pd_index(virt);
    uint32_t *pd = PD_VIRT;

    if (!(pd[pdi] & PAGE_PRESENT)) {
        uint32_t table = pmm_alloc_frame();
        if (!table)
            return false;
        pd[pdi] = table | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
        invlpg((uint32_t)PT_VIRT(pdi));
        memset(PT_VIRT(pdi), 0, PAGE_SIZE);
    }

    PT_VIRT(pdi)[pt_index(virt)] = (phys & ~(PAGE_SIZE - 1)) | (flags & 0xFFFu) | PAGE_PRESENT;
    invlpg(virt);
    return true;
}

bool paging_unmap(uint32_t virt, uint32_t *phys_out)
{
    uint32_t pdi = pd_index(virt);
    if (!(PD_VIRT[pdi] & PAGE_PRESENT))
        return false;

    uint32_t *pte = &PT_VIRT(pdi)[pt_index(virt)];
    if (!(*pte & PAGE_PRESENT))
        return false;

    if (phys_out)
        *phys_out = *pte & ~(PAGE_SIZE - 1);
    *pte = 0;
    invlpg(virt);
    return true;
}

bool paging_query(uint32_t virt, uint32_t *pde, uint32_t *pte)
{
    uint32_t pdi = pd_index(virt);
    *pde = PD_VIRT[pdi];
    *pte = 0;

    if (!(*pde & PAGE_PRESENT))
        return false;

    *pte = PT_VIRT(pdi)[pt_index(virt)];
    return (*pte & PAGE_PRESENT) != 0;
}

static void page_fault_handler(registers_t *regs)
{
    uint32_t addr = read_cr2();
    fault_count++;

    /* Demand paging: first touch of a reserved heap page gets a fresh frame.
     * The test is the heap's reserved range, not the allocator's brk: the
     * faulting store may be the very store that publishes the new brk, so brk
     * is not a dependable boundary here (see heap_grow). */
    if (!(regs->err_code & PF_PRESENT) && heap_reserved(addr)) {
        uint32_t page  = ALIGN_DOWN(addr, PAGE_SIZE);
        uint32_t frame = pmm_alloc_frame();

        if (frame && paging_map(page, frame, PAGE_WRITE)) {
            memset((void *)page, 0, PAGE_SIZE);
            demand_count++;
            return;
        }
        if (frame)
            pmm_free_frame(frame);
        vga_set_color(vga_color(VGA_WHITE, VGA_RED));
        kprintf("\nOut of physical memory while servicing a page fault\n");
    }

    vga_set_color(vga_color(VGA_WHITE, VGA_RED));
    kprintf("\nPage fault at address %p\n", (void *)addr);
    kprintf("  cause: %s, %s access, %s mode%s%s\n",
            (regs->err_code & PF_PRESENT) ? "protection violation" : "page not present",
            (regs->err_code & PF_WRITE)   ? "write" : "read",
            (regs->err_code & PF_USER)    ? "user" : "kernel",
            (regs->err_code & PF_RESERVED) ? ", reserved bit set" : "",
            (regs->err_code & PF_FETCH)    ? ", instruction fetch" : "");
    kernel_panic("Unhandled page fault", regs);
}

void paging_init(void)
{
    uint32_t pd_phys = pmm_alloc_frame();
    if (!pd_phys)
        PANIC("No memory for the page directory");

    /* Paging is still off, so physical addresses are directly usable here. */
    uint32_t *pd = (uint32_t *)pd_phys;
    memset(pd, 0, PAGE_SIZE);

    for (uint32_t pdi = 0; pdi < (IDENTITY_MAP_SIZE >> 22); pdi++) {
        uint32_t table_phys = pmm_alloc_frame();
        if (!table_phys)
            PANIC("No memory for page tables");

        uint32_t *table = (uint32_t *)table_phys;
        for (uint32_t i = 0; i < ENTRIES_PER_TABLE; i++)
            table[i] = ((pdi << 22) | (i << 12)) | PAGE_PRESENT | PAGE_WRITE;

        if (pdi == 0)
            table[0] = 0;                     /* guard page: catches NULL derefs */

        pd[pdi] = table_phys | PAGE_PRESENT | PAGE_WRITE;
    }

    pd[RECURSIVE_INDEX] = pd_phys | PAGE_PRESENT | PAGE_WRITE;

    isr_register_handler(14, page_fault_handler);

    uint32_t cr0;
    __asm__ volatile("mov %0, %%cr3" : : "r"(pd_phys) : "memory");
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= CR0_PAGING | CR0_WRITE_PROTECT;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

uint32_t paging_fault_count(void)  { return fault_count; }
uint32_t paging_demand_count(void) { return demand_count; }
