/* Reinhard OS kernel entry point and panic handler. */

#include "kernel.h"

#include "fs.h"
#include "gdt.h"
#include "heap.h"
#include "idt.h"
#include "io.h"
#include "isr.h"
#include "keyboard.h"
#include "kprintf.h"
#include "multiboot.h"
#include "paging.h"
#include "pic.h"
#include "pmm.h"
#include "shell.h"
#include "timer.h"
#include "vga.h"

#if !defined(__i386__)
#error "This kernel must be compiled for a 32-bit x86 target"
#endif

#define TIMER_HZ 100

void kernel_panic(const char *msg, const registers_t *regs)
{
    cpu_cli();
    vga_set_color(vga_color(VGA_WHITE, VGA_RED));
    kprintf("\n*** KERNEL PANIC: %s ***\n", msg);

    if (regs) {
        kprintf("int=%u err=%08x eip=%08x cs=%04x eflags=%08x\n",
                regs->int_no, regs->err_code, regs->eip, regs->cs, regs->eflags);
        kprintf("eax=%08x ebx=%08x ecx=%08x edx=%08x\n", regs->eax, regs->ebx, regs->ecx, regs->edx);
        kprintf("esi=%08x edi=%08x ebp=%08x esp=%08x\n", regs->esi, regs->edi, regs->ebp, regs->esp);
    }
    kprintf("System halted.\n");
    cpu_halt_forever();
}

static void boot_step(const char *what)
{
    vga_set_color(vga_color(VGA_LIGHT_GREY, VGA_BLACK));
    kprintf("[ ");
    vga_set_color(vga_color(VGA_LIGHT_GREEN, VGA_BLACK));
    kprintf("OK");
    vga_set_color(vga_color(VGA_LIGHT_GREY, VGA_BLACK));
    kprintf(" ] %s\n", what);
}

/* "REINHARD" in a 7x5 block font; the version is printed on the middle row. */
static void print_banner(void)
{
    static const char *const art[7] = {
        "####  ##### ##### #   # #   #   #   ####  #### ",
        "#   # #       #   ##  # #   #  # #  #   # #   #",
        "#   # #       #   # # # #   # #   # #   # #   #",
        "####  ####    #   #  ## ##### #   # ####  #   #",
        "#   # #       #   #   # #   # ##### #   # #   #",
        "#   # #       #   #   # #   # #   # #   # #   #",
        "#   # ##### ##### #   # #   # #   # #   # #### ",
    };

    vga_set_color(vga_color(VGA_LIGHT_CYAN, VGA_BLACK));
    for (size_t i = 0; i < ARRAY_SIZE(art); i++) {
        if (i == 3)
            kprintf("%s   OS %s\n", art[i], REINHARD_VERSION);
        else
            kprintf("%s\n", art[i]);
    }
    kprintf("\n");
    vga_set_color(vga_color(VGA_LIGHT_GREY, VGA_BLACK));
}

void kernel_main(uint32_t magic, const multiboot_info_t *mbi)
{
    vga_init();
    print_banner();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        PANIC("Not booted by a Multiboot-compliant bootloader");

    gdt_init();     boot_step("GDT loaded");
    idt_init();
    pic_init();     boot_step("IDT + PIC initialised");
    pmm_init(mbi);  boot_step("Physical memory manager");
    paging_init();  boot_step("Paging enabled");
    heap_init();    boot_step("Kernel heap (demand paged)");
    timer_init(TIMER_HZ);
    keyboard_init(); boot_step("Timer and keyboard drivers");
    fs_init();      boot_step("RAM filesystem");

    cpu_sti();

    kprintf("\nType 'help' for a list of commands.\n\n");
    shell_run();
}
