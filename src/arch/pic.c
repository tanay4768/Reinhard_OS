#include "pic.h"

#include "io.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI        0x20
#define PIC_READ_ISR   0x0B

#define ICW1_INIT      0x10
#define ICW1_ICW4      0x01
#define ICW4_8086      0x01

#define PIC1_OFFSET    0x20   /* IRQ 0-7  -> vectors 32-39 */
#define PIC2_OFFSET    0x28   /* IRQ 8-15 -> vectors 40-47 */

void pic_init(void)
{
    /* Start initialisation in cascade mode. */
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC1_DATA, PIC1_OFFSET);          io_wait();
    outb(PIC2_DATA, PIC2_OFFSET);          io_wait();
    outb(PIC1_DATA, 4);                    io_wait();   /* slave on IRQ2 */
    outb(PIC2_DATA, 2);                    io_wait();   /* cascade identity */
    outb(PIC1_DATA, ICW4_8086);            io_wait();
    outb(PIC2_DATA, ICW4_8086);            io_wait();

    /* Mask everything; drivers unmask the lines they own. */
    outb(PIC1_DATA, 0xFB);                 /* keep the cascade line (IRQ2) open */
    outb(PIC2_DATA, 0xFF);
}

void pic_unmask(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t  bit  = (irq < 8) ? irq : (uint8_t)(irq - 8);
    outb(port, inb(port) & (uint8_t)~(1u << bit));
}

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

/* IRQ7/IRQ15 can fire spuriously; the in-service register tells us. */
bool pic_irq_is_spurious(uint8_t irq)
{
    if (irq == 7) {
        outb(PIC1_CMD, PIC_READ_ISR);
        return !(inb(PIC1_CMD) & 0x80);
    }
    if (irq == 15) {
        outb(PIC2_CMD, PIC_READ_ISR);
        if (!(inb(PIC2_CMD) & 0x80)) {
            outb(PIC1_CMD, PIC_EOI);   /* master still saw the cascade IRQ */
            return true;
        }
    }
    return false;
}
