#include "hal_gpio.h"

uint8_t HalGpio_IsValid(const HalGpio *gpio)
{
    if ((gpio == 0) || (gpio->ops == 0) || (gpio->ops->write == 0))
    {
        return 0u;
    }

    return 1u;
}

HalStatus HalGpio_Write(const HalGpio *gpio, HalGpioLevel level)
{
    if ((level != HAL_GPIO_LEVEL_LOW) && (level != HAL_GPIO_LEVEL_HIGH))
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!HalGpio_IsValid(gpio))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    return gpio->ops->write(gpio->context, level);
}

HalStatus HalGpio_Read(const HalGpio *gpio, HalGpioLevel *level)
{
    if ((gpio == 0) || (gpio->ops == 0) || (level == 0))
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (gpio->ops->read == 0)
    {
        return HAL_STATUS_UNSUPPORTED;
    }

    return gpio->ops->read(gpio->context, level);
}
