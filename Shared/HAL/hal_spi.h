#ifndef HAL_SPI_H
#define HAL_SPI_H

#include <stdint.h>

#include "hal_status.h"

typedef struct
{
    const uint8_t *transmit_data;
    uint8_t *receive_data;
    uint16_t length;
    uint8_t chip_select_id;
} HalSpiTransfer;

typedef HalStatus (*HalSpiStartFn)(
    void *context,
    const HalSpiTransfer *transfer);
typedef HalStatus (*HalSpiGetStateFn)(
    void *context,
    HalTransferState *state);
typedef HalStatus (*HalSpiCancelFn)(void *context);

typedef struct
{
    HalSpiStartFn start;
    HalSpiGetStateFn get_state;
    HalSpiCancelFn cancel;
} HalSpiOps;

typedef struct
{
    void *context;
    const HalSpiOps *ops;
} HalSpi;

uint8_t HalSpi_IsValid(const HalSpi *spi);
HalStatus HalSpi_Start(
    const HalSpi *spi,
    const HalSpiTransfer *transfer);
HalStatus HalSpi_GetState(
    const HalSpi *spi,
    HalTransferState *state);
HalStatus HalSpi_Cancel(const HalSpi *spi);

#endif
