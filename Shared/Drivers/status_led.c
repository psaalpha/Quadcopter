#include "status_led.h"

static DriverStatus StatusLed_MapHalStatus(HalStatus status)
{
    switch (status)
    {
        case HAL_STATUS_OK:
            return DRIVER_STATUS_OK;
        case HAL_STATUS_INVALID_ARGUMENT:
            return DRIVER_STATUS_INVALID_ARGUMENT;
        case HAL_STATUS_NOT_INITIALIZED:
            return DRIVER_STATUS_NOT_INITIALIZED;
        case HAL_STATUS_BUSY:
            return DRIVER_STATUS_BUSY;
        case HAL_STATUS_UNSUPPORTED:
            return DRIVER_STATUS_UNSUPPORTED;
        case HAL_STATUS_IO_ERROR:
        default:
            return DRIVER_STATUS_IO_ERROR;
    }
}

static HalGpioLevel StatusLed_InactiveLevel(HalGpioLevel active_level)
{
    return (active_level == HAL_GPIO_LEVEL_HIGH)
        ? HAL_GPIO_LEVEL_LOW
        : HAL_GPIO_LEVEL_HIGH;
}

DriverStatus StatusLed_Init(
    StatusLed *led,
    const HalGpio *gpio,
    HalGpioLevel active_level,
    uint32_t timestamp_ms)
{
    DriverStatus status;

    if (led == 0)
    {
        return DRIVER_STATUS_INVALID_ARGUMENT;
    }

    DriverHealth_Init(&led->health);
    DriverHealth_SetState(&led->health, DRIVER_STATE_INITIALIZING);
    led->gpio = gpio;
    led->active_level = active_level;
    led->state = STATUS_LED_OFF;
    led->initialized = 0u;

    if (!HalGpio_IsValid(gpio)
        || ((active_level != HAL_GPIO_LEVEL_LOW)
            && (active_level != HAL_GPIO_LEVEL_HIGH)))
    {
        status = DRIVER_STATUS_INVALID_ARGUMENT;
        DriverHealth_Record(&led->health, status, timestamp_ms);
        DriverHealth_SetState(&led->health, DRIVER_STATE_FAULT);
        return status;
    }

    status = StatusLed_MapHalStatus(
        HalGpio_Write(gpio, StatusLed_InactiveLevel(active_level)));
    DriverHealth_Record(&led->health, status, timestamp_ms);
    if (status != DRIVER_STATUS_OK)
    {
        DriverHealth_SetState(&led->health, DRIVER_STATE_FAULT);
        return status;
    }

    led->initialized = 1u;
    DriverHealth_SetState(&led->health, DRIVER_STATE_READY);
    return DRIVER_STATUS_OK;
}

DriverStatus StatusLed_Set(
    StatusLed *led,
    StatusLedState state,
    uint32_t timestamp_ms)
{
    HalGpioLevel level;
    DriverStatus status;

    if (led == 0)
    {
        return DRIVER_STATUS_INVALID_ARGUMENT;
    }
    if (!led->initialized)
    {
        status = DRIVER_STATUS_NOT_INITIALIZED;
        DriverHealth_Record(&led->health, status, timestamp_ms);
        return status;
    }
    if ((state != STATUS_LED_OFF) && (state != STATUS_LED_ON))
    {
        status = DRIVER_STATUS_INVALID_ARGUMENT;
        DriverHealth_Record(&led->health, status, timestamp_ms);
        return status;
    }

    level = (state == STATUS_LED_ON)
        ? led->active_level
        : StatusLed_InactiveLevel(led->active_level);
    status = StatusLed_MapHalStatus(HalGpio_Write(led->gpio, level));
    DriverHealth_Record(&led->health, status, timestamp_ms);

    if (status == DRIVER_STATUS_OK)
    {
        led->state = state;
        DriverHealth_SetState(&led->health, DRIVER_STATE_READY);
    }
    else
    {
        DriverHealth_SetState(&led->health, DRIVER_STATE_DEGRADED);
    }

    return status;
}

DriverStatus StatusLed_Toggle(StatusLed *led, uint32_t timestamp_ms)
{
    StatusLedState next_state;

    if (led == 0)
    {
        return DRIVER_STATUS_INVALID_ARGUMENT;
    }
    if (!led->initialized)
    {
        DriverHealth_Record(
            &led->health,
            DRIVER_STATUS_NOT_INITIALIZED,
            timestamp_ms);
        return DRIVER_STATUS_NOT_INITIALIZED;
    }

    next_state = (led->state == STATUS_LED_ON)
        ? STATUS_LED_OFF
        : STATUS_LED_ON;
    return StatusLed_Set(led, next_state, timestamp_ms);
}

DriverStatus StatusLed_GetState(
    const StatusLed *led,
    StatusLedState *state)
{
    if ((led == 0) || (state == 0))
    {
        return DRIVER_STATUS_INVALID_ARGUMENT;
    }
    if (!led->initialized)
    {
        return DRIVER_STATUS_NOT_INITIALIZED;
    }

    *state = led->state;
    return DRIVER_STATUS_OK;
}

const DriverHealth *StatusLed_GetHealth(const StatusLed *led)
{
    return (led == 0) ? 0 : &led->health;
}
