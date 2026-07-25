#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>

typedef enum
{
    HAL_STATUS_OK = 0,
    HAL_STATUS_INVALID_ARGUMENT,
    HAL_STATUS_NOT_INITIALIZED,
    HAL_STATUS_BUSY,
    HAL_STATUS_IO_ERROR,
    HAL_STATUS_UNSUPPORTED
} HalStatus;

typedef enum
{
    HAL_GPIO_LEVEL_LOW = 0,
    HAL_GPIO_LEVEL_HIGH = 1
} HalGpioLevel;

typedef HalStatus (*HalGpioWriteFn)(void *context, HalGpioLevel level);
typedef HalStatus (*HalGpioReadFn)(void *context, HalGpioLevel *level);

typedef struct
{
    HalGpioWriteFn write;
    HalGpioReadFn read;
} HalGpioOps;

typedef struct
{
    void *context;
    const HalGpioOps *ops;
} HalGpio;

uint8_t HalGpio_IsValid(const HalGpio *gpio);
HalStatus HalGpio_Write(const HalGpio *gpio, HalGpioLevel level);
HalStatus HalGpio_Read(const HalGpio *gpio, HalGpioLevel *level);

#endif
