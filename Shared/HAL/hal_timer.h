#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include <stdint.h>

#include "hal_status.h"

typedef HalStatus (*HalTimerControlFn)(void *context);
typedef HalStatus (*HalTimerGetU32Fn)(void *context, uint32_t *value);

typedef struct
{
    HalTimerControlFn start;
    HalTimerControlFn stop;
    HalTimerGetU32Fn get_counter_ticks;
    HalTimerGetU32Fn get_frequency_hz;
} HalTimerOps;

typedef struct
{
    void *context;
    const HalTimerOps *ops;
} HalTimer;

uint8_t HalTimer_IsValid(const HalTimer *timer);
HalStatus HalTimer_Start(const HalTimer *timer);
HalStatus HalTimer_Stop(const HalTimer *timer);
HalStatus HalTimer_GetCounterTicks(
    const HalTimer *timer,
    uint32_t *counter_ticks);
HalStatus HalTimer_GetFrequencyHz(
    const HalTimer *timer,
    uint32_t *frequency_hz);

#endif
