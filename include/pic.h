#ifndef AKIRA_PIC_H
#define AKIRA_PIC_H

#include <stdbool.h>
#include <stdint.h>

void pic_init(void);
void pic_unmask(uint8_t irq);
void pic_send_eoi(uint8_t irq);
bool pic_irq_is_spurious(uint8_t irq);

#endif
