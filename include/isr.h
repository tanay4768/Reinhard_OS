#ifndef AKIRA_ISR_H
#define AKIRA_ISR_H

#include <stdint.h>

#define IRQ_BASE 32

/* Layout must match the push order in isr_stubs.s. */
typedef struct registers {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;   /* pusha */
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;                           /* pushed by the CPU */
} registers_t;

typedef void (*isr_handler_t)(registers_t *regs);

void isr_register_handler(uint8_t int_no, isr_handler_t handler);

#endif
