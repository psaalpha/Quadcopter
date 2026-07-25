#ifndef STM32F1_GPIO_HAL_H
#define STM32F1_GPIO_HAL_H

#include "stm32f10x.h"

#include "hal_gpio.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} Stm32f1GpioOutput;

HalStatus Stm32f1GpioOutput_Bind(
    Stm32f1GpioOutput *output,
    GPIO_TypeDef *port,
    uint16_t pin,
    HalGpio *gpio);

#endif
