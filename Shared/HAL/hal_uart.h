/**
 * @file hal_uart.h
 * @brief Non-blocking hardware-independent UART transaction contract.
 */
#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>

#include "hal_status.h"

typedef HalStatus (*HalUartStartTransmitFn)(
    void *context,
    const uint8_t *data,
    uint16_t length);
typedef HalStatus (*HalUartReadFn)(
    void *context,
    uint8_t *data,
    uint16_t capacity,
    uint16_t *read_length);
typedef HalStatus (*HalUartGetStateFn)(
    void *context,
    HalTransferState *state);
typedef HalStatus (*HalUartAbortFn)(void *context);

typedef struct
{
    HalUartStartTransmitFn start_transmit;
    HalUartReadFn read;
    HalUartGetStateFn get_tx_state;
    HalUartAbortFn abort_transmit;
} HalUartOps;

typedef struct
{
    void *context;
    const HalUartOps *ops;
} HalUart;

uint8_t HalUart_IsValid(const HalUart *uart);
HalStatus HalUart_StartTransmit(
    const HalUart *uart,
    const uint8_t *data,
    uint16_t length);
HalStatus HalUart_Read(
    const HalUart *uart,
    uint8_t *data,
    uint16_t capacity,
    uint16_t *read_length);
HalStatus HalUart_GetTransmitState(
    const HalUart *uart,
    HalTransferState *state);
HalStatus HalUart_AbortTransmit(const HalUart *uart);

#endif
