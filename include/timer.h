#ifndef AKIRA_TIMER_H
#define AKIRA_TIMER_H

#include <stdint.h>

void     timer_init(uint32_t hz);
uint32_t timer_ticks(void);
uint32_t timer_hz(void);
void     timer_sleep_ms(uint32_t ms);

#endif
