#include "fake_hal_gpio.h"

static HalStatus FakeHalGpio_Write(void *context, HalGpioLevel level)
{
    FakeHalGpio *fake = (FakeHalGpio *)context;
    HalStatus status;

    if (fake == 0)
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }

    fake->write_count++;
    status = fake->next_write_status;
    fake->next_write_status = HAL_STATUS_OK;
    if (status == HAL_STATUS_OK)
    {
        fake->level = level;
    }
    return status;
}

static HalStatus FakeHalGpio_Read(void *context, HalGpioLevel *level)
{
    FakeHalGpio *fake = (FakeHalGpio *)context;
    HalStatus status;

    if ((fake == 0) || (level == 0))
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }

    fake->read_count++;
    status = fake->next_read_status;
    fake->next_read_status = HAL_STATUS_OK;
    if (status == HAL_STATUS_OK)
    {
        *level = fake->level;
    }
    return status;
}

static const HalGpioOps fake_hal_gpio_ops =
{
    FakeHalGpio_Write,
    FakeHalGpio_Read
};

void FakeHalGpio_Init(FakeHalGpio *fake, HalGpioLevel initial_level)
{
    if (fake == 0)
    {
        return;
    }

    fake->gpio.context = fake;
    fake->gpio.ops = &fake_hal_gpio_ops;
    fake->level = initial_level;
    fake->next_write_status = HAL_STATUS_OK;
    fake->next_read_status = HAL_STATUS_OK;
    fake->write_count = 0u;
    fake->read_count = 0u;
}

void FakeHalGpio_FailNextWrite(FakeHalGpio *fake, HalStatus status)
{
    if (fake != 0)
    {
        fake->next_write_status = status;
    }
}

void FakeHalGpio_FailNextRead(FakeHalGpio *fake, HalStatus status)
{
    if (fake != 0)
    {
        fake->next_read_status = status;
    }
}
