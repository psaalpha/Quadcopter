#include "hal_spi.h"

uint8_t HalSpi_IsValid(const HalSpi *spi)
{
    return ((spi != 0) && (spi->ops != 0)
        && (spi->ops->start != 0)
        && (spi->ops->get_state != 0))
        ? 1u
        : 0u;
}

HalStatus HalSpi_Start(
    const HalSpi *spi,
    const HalSpiTransfer *transfer)
{
    if ((transfer == 0) || (transfer->length == 0u)
        || ((transfer->transmit_data == 0)
            && (transfer->receive_data == 0)))
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!HalSpi_IsValid(spi))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    return spi->ops->start(spi->context, transfer);
}

HalStatus HalSpi_GetState(
    const HalSpi *spi,
    HalTransferState *state)
{
    if (state == 0)
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!HalSpi_IsValid(spi))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    return spi->ops->get_state(spi->context, state);
}

HalStatus HalSpi_Cancel(const HalSpi *spi)
{
    if (!HalSpi_IsValid(spi))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }
    if (spi->ops->cancel == 0)
    {
        return HAL_STATUS_UNSUPPORTED;
    }

    return spi->ops->cancel(spi->context);
}
