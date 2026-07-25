#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <stdint.h>

#include "hal_status.h"

typedef struct
{
    uint8_t address_7bit;
    const uint8_t *write_data;
    uint16_t write_length;
    uint8_t *read_data;
    uint16_t read_length;
} HalI2cTransfer;

typedef HalStatus (*HalI2cStartFn)(
    void *context,
    const HalI2cTransfer *transfer);
typedef HalStatus (*HalI2cGetStateFn)(
    void *context,
    HalTransferState *state);
typedef HalStatus (*HalI2cCancelFn)(void *context);

typedef struct
{
    HalI2cStartFn start;
    HalI2cGetStateFn get_state;
    HalI2cCancelFn cancel;
} HalI2cOps;

typedef struct
{
    void *context;
    const HalI2cOps *ops;
} HalI2c;

uint8_t HalI2c_IsValid(const HalI2c *i2c);
HalStatus HalI2c_Start(
    const HalI2c *i2c,
    const HalI2cTransfer *transfer);
HalStatus HalI2c_GetState(
    const HalI2c *i2c,
    HalTransferState *state);
HalStatus HalI2c_Cancel(const HalI2c *i2c);

#endif
