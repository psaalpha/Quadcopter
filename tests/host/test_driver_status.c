#include "driver_status.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

static void TestInitialState(void)
{
    DriverHealth health;

    DriverHealth_Init(&health);

    assert(health.state == DRIVER_STATE_UNINITIALIZED);
    assert(health.last_status == DRIVER_STATUS_NOT_INITIALIZED);
    assert(health.successful_operations == 0u);
    assert(health.error_count == 0u);
    assert(health.consecutive_errors == 0u);
    assert(DriverStatus_IsOk(DRIVER_STATUS_OK) == 1u);
    assert(DriverStatus_IsOk(DRIVER_STATUS_TIMEOUT) == 0u);
}

static void TestResultsDoNotHideLifecyclePolicy(void)
{
    DriverHealth health;

    DriverHealth_Init(&health);
    DriverHealth_SetState(&health, DRIVER_STATE_READY);
    DriverHealth_Record(&health, DRIVER_STATUS_TIMEOUT, 25u);
    DriverHealth_Record(&health, DRIVER_STATUS_IO_ERROR, 30u);

    assert(health.state == DRIVER_STATE_READY);
    assert(health.last_status == DRIVER_STATUS_IO_ERROR);
    assert(health.error_count == 2u);
    assert(health.consecutive_errors == 2u);
    assert(health.last_error_ms == 30u);

    DriverHealth_SetState(&health, DRIVER_STATE_DEGRADED);
    DriverHealth_Record(&health, DRIVER_STATUS_OK, 35u);

    assert(health.state == DRIVER_STATE_DEGRADED);
    assert(health.last_status == DRIVER_STATUS_OK);
    assert(health.successful_operations == 1u);
    assert(health.consecutive_errors == 0u);
    assert(health.last_success_ms == 35u);
}

static void TestCountersSaturate(void)
{
    DriverHealth health;

    DriverHealth_Init(&health);
    health.error_count = UINT32_MAX;
    health.consecutive_errors = UINT32_MAX;

    DriverHealth_Record(&health, DRIVER_STATUS_CRC_ERROR, 100u);

    assert(health.error_count == UINT32_MAX);
    assert(health.consecutive_errors == UINT32_MAX);
}

int main(void)
{
    TestInitialState();
    TestResultsDoNotHideLifecyclePolicy();
    TestCountersSaturate();

    puts("driver status tests passed");
    return 0;
}
