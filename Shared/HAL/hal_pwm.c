#include "hal_pwm.h"

uint8_t HalPwm_IsValid(const HalPwm *pwm)
{
    return ((pwm != 0) && (pwm->ops != 0)
        && (pwm->ops->set_pulse_ticks != 0)
        && (pwm->ops->enable != 0)
        && (pwm->ops->get_limits != 0))
        ? 1u
        : 0u;
}

HalStatus HalPwm_SetPulseTicks(
    const HalPwm *pwm,
    uint8_t channel,
    uint32_t pulse_ticks)
{
    HalPwmLimits limits;
    HalStatus status;

    if (!HalPwm_IsValid(pwm))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    status = pwm->ops->get_limits(pwm->context, channel, &limits);
    if (status != HAL_STATUS_OK)
    {
        return status;
    }
    if ((pulse_ticks < limits.minimum_ticks)
        || (pulse_ticks > limits.maximum_ticks))
    {
        return HAL_STATUS_OUT_OF_RANGE;
    }

    return pwm->ops->set_pulse_ticks(
        pwm->context,
        channel,
        pulse_ticks);
}

HalStatus HalPwm_Enable(
    const HalPwm *pwm,
    uint8_t channel,
    uint8_t enabled)
{
    if (!HalPwm_IsValid(pwm))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }
    if (enabled > 1u)
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }

    return pwm->ops->enable(pwm->context, channel, enabled);
}

HalStatus HalPwm_GetLimits(
    const HalPwm *pwm,
    uint8_t channel,
    HalPwmLimits *limits)
{
    if (limits == 0)
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!HalPwm_IsValid(pwm))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    return pwm->ops->get_limits(pwm->context, channel, limits);
}
