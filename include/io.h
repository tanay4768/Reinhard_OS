#ifndef AKIRA_IO_H
#define AKIRA_IO_H

#include <stdint.h>

/* ---- x86 port I/O ---- */
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline void io_wait(void)
{
    outb(0x80, 0);
}

/* ---- CPU helpers ---- */
static inline void cpu_sti(void) { __asm__ volatile("sti" : : : "memory"); }
static inline void cpu_cli(void) { __asm__ volatile("cli" : : : "memory"); }

static inline __attribute__((noreturn)) void cpu_halt_forever(void)
{
    for (;;)
        __asm__ volatile("cli; hlt");
}

static inline uint32_t read_cr2(void)
{
    uint32_t v;
    __asm__ volatile("mov %%cr2, %0" : "=r"(v));
    return v;
}

static inline void invlpg(uint32_t virt)
{
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

#endif
