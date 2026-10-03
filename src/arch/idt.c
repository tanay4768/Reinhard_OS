#include "idt.h"

#include "gdt.h"
#include "kstring.h"

#include <stdint.h>

#define IDT_ENTRIES      256
#define IDT_VECTORS_USED 48            /* 32 exceptions + 16 IRQs */
#define IDT_GATE_INT32   0x8E          /* present, ring 0, 32-bit interrupt gate */

typedef struct {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

extern uint32_t isr_stub_table[];      /* defined in isr_stubs.s */

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idt_ptr;

static void idt_set_gate(uint8_t vector, uint32_t handler)
{
    idt[vector].base_low  = handler & 0xFFFF;
    idt[vector].base_high = (handler >> 16) & 0xFFFF;
    idt[vector].selector  = GDT_KERNEL_CODE;
    idt[vector].zero      = 0;
    idt[vector].flags     = IDT_GATE_INT32;
}

void idt_init(void)
{
    memset(idt, 0, sizeof(idt));

    for (int i = 0; i < IDT_VECTORS_USED; i++)
        idt_set_gate((uint8_t)i, isr_stub_table[i]);

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (uint32_t)&idt;
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}
