#include "stm32f1_gpio_hal.h"

static HalStatus Stm32f1GpioOutput_Write(
    void *context,
    HalGpioLevel level)
{
    Stm32f1GpioOutput *output = (Stm32f1GpioOutput *)context;

    if ((output == 0) || (output->port == 0) || (output->pin == 0u))
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }

    if (level == HAL_GPIO_LEVEL_HIGH)
    {
        GPIO_SetBits(output->port, output->pin);
    }
    else if (level == HAL_GPIO_LEVEL_LOW)
    {
        GPIO_ResetBits(output->port, output->pin);
    }
    else
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }

    return HAL_STATUS_OK;
}

static HalStatus Stm32f1GpioOutput_Read(
    void *context,
    HalGpioLevel *level)
{
    Stm32f1GpioOutput *output = (Stm32f1GpioOutput *)context;

    if ((output == 0) || (output->port == 0)
        || (output->pin == 0u) || (level == 0))
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }

    *level = (GPIO_ReadOutputDataBit(output->port, output->pin) != Bit_RESET)
        ? HAL_GPIO_LEVEL_HIGH
        : HAL_GPIO_LEVEL_LOW;
    return HAL_STATUS_OK;
}

static const HalGpioOps stm32f1_gpio_output_ops =
{
    Stm32f1GpioOutput_Write,
    Stm32f1GpioOutput_Read
};

HalStatus Stm32f1GpioOutput_Bind(
    Stm32f1GpioOutput *output,
    GPIO_TypeDef *port,
    uint16_t pin,
    HalGpio *gpio)
{
    if ((output == 0) || (port == 0) || (pin == 0u) || (gpio == 0))
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }

    output->port = port;
    output->pin = pin;
    gpio->context = output;
    gpio->ops = &stm32f1_gpio_output_ops;
    return HAL_STATUS_OK;
}
