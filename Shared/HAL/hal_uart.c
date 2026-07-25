#include "hal_uart.h"

uint8_t HalUart_IsValid(const HalUart *uart)
{
    return ((uart != 0) && (uart->ops != 0)
        && (uart->ops->start_transmit != 0)
        && (uart->ops->read != 0)
        && (uart->ops->get_tx_state != 0))
        ? 1u
        : 0u;
}

HalStatus HalUart_StartTransmit(
    const HalUart *uart,
    const uint8_t *data,
    uint16_t length)
{
    if ((data == 0) || (length == 0u))
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!HalUart_IsValid(uart))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    return uart->ops->start_transmit(uart->context, data, length);
}

HalStatus HalUart_Read(
    const HalUart *uart,
    uint8_t *data,
    uint16_t capacity,
    uint16_t *read_length)
{
    if ((data == 0) || (capacity == 0u) || (read_length == 0))
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!HalUart_IsValid(uart))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    *read_length = 0u;
    return uart->ops->read(
        uart->context,
        data,
        capacity,
        read_length);
}

HalStatus HalUart_GetTransmitState(
    const HalUart *uart,
    HalTransferState *state)
{
    if (state == 0)
    {
        return HAL_STATUS_INVALID_ARGUMENT;
    }
    if (!HalUart_IsValid(uart))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }

    return uart->ops->get_tx_state(uart->context, state);
}

HalStatus HalUart_AbortTransmit(const HalUart *uart)
{
    if (!HalUart_IsValid(uart))
    {
        return HAL_STATUS_NOT_INITIALIZED;
    }
    if (uart->ops->abort_transmit == 0)
    {
        return HAL_STATUS_UNSUPPORTED;
    }

    return uart->ops->abort_transmit(uart->context);
}
