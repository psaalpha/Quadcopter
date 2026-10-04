#ifndef MILLISECOND_CLOCK_H
#define MILLISECOND_CLOCK_H
#include "stm32f10x.h"
void MillisecondClock_Init(void);
/* TIM2 ISR only. Includes time spent with IRQs masked (e.g. flash save).
 * Call at least once per CYCCNT wrap (~59s at 72MHz). No clock changes
 * or sleep mode are supported while the application runs.
 */
uint32_t MillisecondClock_Elapsed(void);
#endif
