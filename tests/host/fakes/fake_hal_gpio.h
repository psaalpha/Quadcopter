#ifndef FAKE_HAL_GPIO_H
#define FAKE_HAL_GPIO_H

#include <stdint.h>

#include "hal_gpio.h"

typedef struct
{
    HalGpio gpio;
    HalGpioLevel level;
    HalStatus next_write_status;
    HalStatus next_read_status;
    uint32_t write_count;
    uint32_t read_count;
} FakeHalGpio;

void FakeHalGpio_Init(FakeHalGpio *fake, HalGpioLevel initial_level);
void FakeHalGpio_FailNextWrite(FakeHalGpio *fake, HalStatus status);
void FakeHalGpio_FailNextRead(FakeHalGpio *fake, HalStatus status);

#endif
