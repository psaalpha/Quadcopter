#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <stdint.h>

#include "driver_status.h"
#include "hal_gpio.h"

typedef enum
{
    STATUS_LED_OFF = 0,
    STATUS_LED_ON = 1
} StatusLedState;

typedef struct
{
    const HalGpio *gpio;
    HalGpioLevel active_level;
    StatusLedState state;
    uint8_t initialized;
    DriverHealth health;
} StatusLed;

DriverStatus StatusLed_Init(
    StatusLed *led,
    const HalGpio *gpio,
    HalGpioLevel active_level,
    uint32_t timestamp_ms);
DriverStatus StatusLed_Set(
    StatusLed *led,
    StatusLedState state,
    uint32_t timestamp_ms);
DriverStatus StatusLed_Toggle(StatusLed *led, uint32_t timestamp_ms);
DriverStatus StatusLed_GetState(
    const StatusLed *led,
    StatusLedState *state);
const DriverHealth *StatusLed_GetHealth(const StatusLed *led);

#endif
