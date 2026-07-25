/**
 * @file hal_status.h
 * @brief Common hardware-independent result codes shared by HAL contracts.
 */
#ifndef HAL_STATUS_H
#define HAL_STATUS_H

typedef enum
{
    HAL_STATUS_OK = 0,
    HAL_STATUS_INVALID_ARGUMENT,
    HAL_STATUS_NOT_INITIALIZED,
    HAL_STATUS_BUSY,
    HAL_STATUS_IO_ERROR,
    HAL_STATUS_UNSUPPORTED,
    HAL_STATUS_OUT_OF_RANGE,
    HAL_STATUS_TIMEOUT
} HalStatus;

typedef enum
{
    HAL_TRANSFER_IDLE = 0,
    HAL_TRANSFER_BUSY,
    HAL_TRANSFER_COMPLETE,
    HAL_TRANSFER_ERROR
} HalTransferState;

#endif
