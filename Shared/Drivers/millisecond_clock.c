#include "millisecond_clock.h"
/* Legacy CMSIS has no DWT typedef. Cortex-M3 TRM DDI0337 defines these
 * registers. Keep an existing debugger cycle count.
 */
#define CLOCK_DWT_CTRL   (*((volatile uint32_t *)0xE0001000u))
#define CLOCK_DWT_CYCCNT (*((volatile uint32_t *)0xE0001004u))
static uint32_t last_cycles;
static uint32_t cycles_per_ms;
static uint32_t fraction_cycles;
void MillisecondClock_Init(void)
{
    SystemCoreClockUpdate();
    cycles_per_ms = SystemCoreClock / 1000u;
    fraction_cycles = 0u;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    CLOCK_DWT_CTRL |= 1u;
    last_cycles = CLOCK_DWT_CYCCNT;
}
uint32_t MillisecondClock_Elapsed(void)
{
    uint32_t now = CLOCK_DWT_CYCCNT;
    uint32_t delta = now - last_cycles;
    uint32_t elapsed = delta / cycles_per_ms;
    last_cycles = now;
    fraction_cycles += delta % cycles_per_ms;
    if (fraction_cycles >= cycles_per_ms) {
        fraction_cycles -= cycles_per_ms;
        elapsed++;
    }
    return elapsed;
}
