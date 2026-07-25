#ifndef HAL_PWM_H
#define HAL_PWM_H

#include <stdint.h>

#include "hal_status.h"

typedef struct
{
    uint32_t minimum_ticks;
    uint32_t maximum_ticks;
} HalPwmLimits;

typedef HalStatus (*HalPwmSetPulseFn)(
    void *context,
    uint8_t channel,
    uint32_t pulse_ticks);
typedef HalStatus (*HalPwmEnableFn)(
    void *context,
    uint8_t channel,
    uint8_t enabled);
typedef HalStatus (*HalPwmGetLimitsFn)(
    void *context,
    uint8_t channel,
    HalPwmLimits *limits);

typedef struct
{
    HalPwmSetPulseFn set_pulse_ticks;
    HalPwmEnableFn enable;
    HalPwmGetLimitsFn get_limits;
} HalPwmOps;

typedef struct
{
    void *context;
    const HalPwmOps *ops;
} HalPwm;

uint8_t HalPwm_IsValid(const HalPwm *pwm);
HalStatus HalPwm_SetPulseTicks(
    const HalPwm *pwm,
    uint8_t channel,
    uint32_t pulse_ticks);
HalStatus HalPwm_Enable(
    const HalPwm *pwm,
    uint8_t channel,
    uint8_t enabled);
HalStatus HalPwm_GetLimits(
    const HalPwm *pwm,
    uint8_t channel,
    HalPwmLimits *limits);

#endif
