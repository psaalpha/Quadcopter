#include "hal_i2c.h"

uint8_t HalI2c_IsValid(const HalI2c *i2c)
{
    return ((i2c != 0) && (i2c->ops != 0)
        && (i2c->ops->start != 0)
        && (i2c->ops->get_state != 0))
        ? 1u
        : 0u;
}

HalStatus HalI2c_Start(
    const HalI2c *i2c,
    const HalI2cTransfer *transfer)
{
    if ((transfer == 0) || (transfer->address_7bit > 0x7Fu)
        || ((transfer->write_length == 0u)
            && (transfer->read_length == 0u))
        || ((transfer->write_length != 0u)
            && (transfer->write_data == 0))
        || ((transfer->read_length != 0u)
            && (transfer->read_data == 0)))
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!HalI2c_IsValid(i2c))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    return i2c->ops->start(i2c->context, transfer);
}

HalStatus HalI2c_GetState(
    const HalI2c *i2c,
    HalTransferState *state)
{
    if (state == 0)
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!HalI2c_IsValid(i2c))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    return i2c->ops->get_state(i2c->context, state);
}

HalStatus HalI2c_Cancel(const HalI2c *i2c)
{
    if (!HalI2c_IsValid(i2c))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }
    if (i2c->ops->cancel == 0)
    {
        return HAL_STATUS_UNSUPPORTED;
    }

    return i2c->ops->cancel(i2c->context);
}
