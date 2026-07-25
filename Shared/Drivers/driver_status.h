#ifndef DRIVER_STATUS_H
#define DRIVER_STATUS_H

#include <stdint.h>

/*
 * Common result codes for device-driver public APIs.
 *
 * Zero always means success. Callers must not depend on the numeric value of
 * an error; they should compare with the named constant.
 */
typedef enum
{
    DRIVER_STATUS_OK = 0,
    DRIVER_STATUS_INVALID_ARGUMENT,
    DRIVER_STATUS_NOT_INITIALIZED,
    DRIVER_STATUS_NOT_READY,
    DRIVER_STATUS_BUSY,
    DRIVER_STATUS_TIMEOUT,
    DRIVER_STATUS_IO_ERROR,
    DRIVER_STATUS_CRC_ERROR,
    DRIVER_STATUS_OUT_OF_RANGE,
    DRIVER_STATUS_UNSUPPORTED
} DriverStatus;

/*
 * Lifecycle is explicit. Recording an operation result does not silently
 * change the lifecycle state; the owning driver decides its recovery policy.
 */
typedef enum
{
    DRIVER_STATE_UNINITIALIZED = 0,
    DRIVER_STATE_INITIALIZING,
    DRIVER_STATE_READY,
    DRIVER_STATE_DEGRADED,
    DRIVER_STATE_FAULT,
    DRIVER_STATE_STOPPED
} DriverState;

typedef struct
{
    DriverState state;
    DriverStatus last_status;
    uint32_t successful_operations;
    uint32_t error_count;
    uint32_t consecutive_errors;
    uint32_t last_success_ms;
    uint32_t last_error_ms;
} DriverHealth;

void DriverHealth_Init(DriverHealth *health);
void DriverHealth_SetState(DriverHealth *health, DriverState state);
void DriverHealth_Record(
    DriverHealth *health,
    DriverStatus status,
    uint32_t timestamp_ms);
uint8_t DriverStatus_IsOk(DriverStatus status);

#endif
