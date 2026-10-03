#include "isr.h"

#include "kernel.h"
#include "pic.h"

static isr_handler_t handlers[256];

static const char *const exception_names[32] = {
    "Divide error",            "Debug",                    "Non-maskable interrupt",
    "Breakpoint",              "Overflow",                 "Bound range exceeded",
    "Invalid opcode",          "Device not available",     "Double fault",
    "Coprocessor overrun",     "Invalid TSS",              "Segment not present",
    "Stack-segment fault",     "General protection fault", "Page fault",
    "Reserved",                "x87 floating-point error", "Alignment check",
    "Machine check",           "SIMD floating-point error","Virtualization exception",
    "Reserved",                "Reserved",                 "Reserved",
    "Reserved",                "Reserved",                 "Reserved",
    "Reserved",                "Reserved",                 "Reserved",
    "Security exception",      "Reserved",
};

void isr_register_handler(uint8_t int_no, isr_handler_t handler)
{
    handlers[int_no] = handler;
}

/* Called from isr_common (isr_stubs.s) for every interrupt and exception. */
void isr_handler(registers_t *regs)
{
    if (regs->int_no < IRQ_BASE) {
        if (handlers[regs->int_no])
            handlers[regs->int_no](regs);
        else
            kernel_panic(exception_names[regs->int_no], regs);
        return;
    }

    uint8_t irq = (uint8_t)(regs->int_no - IRQ_BASE);
    if (pic_irq_is_spurious(irq))
        return;

    if (handlers[regs->int_no])
        handlers[regs->int_no](regs);
    pic_send_eoi(irq);
}
