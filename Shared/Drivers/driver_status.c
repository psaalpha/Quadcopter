#include "driver_status.h"

#include <limits.h>

static void DriverHealth_Increment(uint32_t *counter)
{
    if (*counter != UINT32_MAX)
    {
        (*counter)++;
    }
}

void DriverHealth_Init(DriverHealth *health)
{
    if (health == 0)
    {
        return;
    }

    health->state = DRIVER_STATE_UNINITIALIZED;
    health->last_status = DRIVER_STATUS_NOT_INITIALIZED;
    health->successful_operations = 0u;
    health->error_count = 0u;
    health->consecutive_errors = 0u;
    health->last_success_ms = 0u;
    health->last_error_ms = 0u;
}

void DriverHealth_SetState(DriverHealth *health, DriverState state)
{
    if (health == 0)
    {
        return;
    }

    health->state = state;
}

void DriverHealth_Record(
    DriverHealth *health,
    DriverStatus status,
    uint32_t timestamp_ms)
{
    if (health == 0)
    {
        return;
    }

    health->last_status = status;
    if (status == DRIVER_STATUS_OK)
    {
        DriverHealth_Increment(&health->successful_operations);
        health->consecutive_errors = 0u;
        health->last_success_ms = timestamp_ms;
    }
    else
    {
        DriverHealth_Increment(&health->error_count);
        DriverHealth_Increment(&health->consecutive_errors);
        health->last_error_ms = timestamp_ms;
    }
}

uint8_t DriverStatus_IsOk(DriverStatus status)
{
    return (status == DRIVER_STATUS_OK) ? 1u : 0u;
}
