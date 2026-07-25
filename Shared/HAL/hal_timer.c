#include "hal_timer.h"

uint8_t HalTimer_IsValid(const HalTimer *timer)
{
    return ((timer != 0) && (timer->ops != 0)
        && (timer->ops->start != 0)
        && (timer->ops->stop != 0)
        && (timer->ops->get_counter_ticks != 0)
        && (timer->ops->get_frequency_hz != 0))
        ? 1u
        : 0u;
}

HalStatus HalTimer_Start(const HalTimer *timer)
{
    if (!HalTimer_IsValid(timer))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }
    return timer->ops->start(timer->context);
}

HalStatus HalTimer_Stop(const HalTimer *timer)
{
    if (!HalTimer_IsValid(timer))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }
    return timer->ops->stop(timer->context);
}

HalStatus HalTimer_GetCounterTicks(
    const HalTimer *timer,
    uint32_t *counter_ticks)
{
    if (counter_ticks == 0)
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!HalTimer_IsValid(timer))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }
    return timer->ops->get_counter_ticks(timer->context, counter_ticks);
}

HalStatus HalTimer_GetFrequencyHz(
    const HalTimer *timer,
    uint32_t *frequency_hz)
{
    if (frequency_hz == 0)
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!HalTimer_IsValid(timer))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }
    return timer->ops->get_frequency_hz(timer->context, frequency_hz);
}
