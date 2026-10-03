#include "timer.h"

#include "io.h"
#include "isr.h"
#include "pic.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_HZ  1193182u

static volatile uint32_t ticks;
static uint32_t          frequency;

static void timer_irq(registers_t *regs)
{
    (void)regs;
    ticks++;
}

void timer_init(uint32_t hz)
{
    frequency = hz;
    uint32_t divisor = PIT_BASE_HZ / hz;

    outb(PIT_COMMAND, 0x36);                         /* channel 0, lobyte/hibyte, rate generator */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    isr_register_handler(IRQ_BASE + 0, timer_irq);
    pic_unmask(0);
}

uint32_t timer_ticks(void) { return ticks; }
uint32_t timer_hz(void)    { return frequency; }

void timer_sleep_ms(uint32_t ms)
{
    uint32_t target = ticks + (ms * frequency) / 1000 + 1;
    while ((int32_t)(target - ticks) > 0)
        __asm__ volatile("sti; hlt");
}
