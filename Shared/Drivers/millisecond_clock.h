#ifndef SHARED_DRIVERS_MILLISECOND_CLOCK_H
#define SHARED_DRIVERS_MILLISECOND_CLOCK_H
#include "stm32f10x.h"
void millisecond_clock_init(void);
/* TIM2 ISR only. Includes time spent with IRQs masked (e.g. flash save).
 * Call at least once per CYCCNT wrap (~59s at 72MHz). No clock changes
 * or sleep mode are supported while the application runs.
 */
uint32_t millisecond_clock_elapsed_ms(void);
#endif
