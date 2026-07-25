#include "watchdog_manager.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef struct
{
    uint32_t reset_mask;
    uint32_t configured_timeout_ms;
    uint32_t init_count;
    uint32_t feed_count;
    uint32_t clear_count;
    WatchdogStatus init_status;
    WatchdogStatus feed_status;
} FakeWatchdog;

static WatchdogStatus FakeWatchdog_Init(
    void *context,
    uint32_t timeout_ms)
{
    FakeWatchdog *fake = (FakeWatchdog *)context;

    fake->init_count++;
    fake->configured_timeout_ms = timeout_ms;
    return fake->init_status;
}

static WatchdogStatus FakeWatchdog_Feed(void *context)
{
    FakeWatchdog *fake = (FakeWatchdog *)context;

    fake->feed_count++;
    return fake->feed_status;
}

static uint32_t FakeWatchdog_ReadReset(void *context)
{
    return ((FakeWatchdog *)context)->reset_mask;
}

static void FakeWatchdog_ClearReset(void *context)
{
    ((FakeWatchdog *)context)->clear_count++;
}

static WatchdogBackend MakeBackend(FakeWatchdog *fake)
{
    WatchdogBackend backend;

    backend.init = FakeWatchdog_Init;
    backend.feed = FakeWatchdog_Feed;
    backend.read_reset_reasons = FakeWatchdog_ReadReset;
    backend.clear_reset_reasons = FakeWatchdog_ClearReset;
    backend.context = fake;
    return backend;
}

static void TestFeedRequiresHealthyClients(void)
{
    FakeWatchdog fake = {0};
    WatchdogBackend backend;
    WatchdogManager manager;
    const WatchdogResetRecord *reset_record;
    uint8_t expired = 0u;

    fake.reset_mask =
        WATCHDOG_RESET_REASON_POWER_ON
        | WATCHDOG_RESET_REASON_INDEPENDENT_WATCHDOG;
    backend = MakeBackend(&fake);

    assert(WatchdogManager_Init(
        &manager,
        &backend,
        1000u,
        5u) == WATCHDOG_STATUS_OK);
    assert(fake.init_count == 1u);
    assert(fake.configured_timeout_ms == 1000u);
    assert(fake.clear_count == 1u);

    reset_record = WatchdogManager_GetResetRecord(&manager);
    assert(reset_record != 0);
    assert(reset_record->reason_mask == fake.reset_mask);
    assert(reset_record->captured_at_ms == 5u);

    assert(WatchdogManager_RegisterClient(
        &manager,
        0u,
        10u,
        1u) == WATCHDOG_STATUS_OK);
    assert(WatchdogManager_RegisterClient(
        &manager,
        1u,
        20u,
        0u) == WATCHDOG_STATUS_OK);

    assert(WatchdogManager_Feed(
        &manager,
        100u) == WATCHDOG_STATUS_CLIENT_NOT_READY);
    assert(fake.feed_count == 0u);

    assert(WatchdogManager_Heartbeat(
        &manager,
        0u,
        100u) == WATCHDOG_STATUS_OK);
    assert(WatchdogManager_Check(
        &manager,
        110u,
        &expired) == WATCHDOG_STATUS_OK);
    assert(expired == 0u);
    assert(WatchdogManager_Feed(&manager, 110u) == WATCHDOG_STATUS_OK);
    assert(fake.feed_count == 1u);

    assert(WatchdogManager_Check(
        &manager,
        111u,
        &expired) == WATCHDOG_STATUS_CLIENT_EXPIRED);
    assert(expired == 0x01u);
    assert(WatchdogManager_Feed(
        &manager,
        111u) == WATCHDOG_STATUS_CLIENT_EXPIRED);
    assert(fake.feed_count == 1u);
    assert(manager.denied_feed_count == 2u);
}

static void TestUnsignedTickWrap(void)
{
    FakeWatchdog fake = {0};
    WatchdogBackend backend = MakeBackend(&fake);
    WatchdogManager manager;
    uint8_t expired = 0u;

    assert(WatchdogManager_Init(
        &manager,
        &backend,
        100u,
        0u) == WATCHDOG_STATUS_OK);
    assert(WatchdogManager_RegisterClient(
        &manager,
        0u,
        10u,
        1u) == WATCHDOG_STATUS_OK);
    assert(WatchdogManager_Heartbeat(
        &manager,
        0u,
        UINT32_MAX - 5u) == WATCHDOG_STATUS_OK);

    assert(WatchdogManager_Check(
        &manager,
        3u,
        &expired) == WATCHDOG_STATUS_OK);
    assert(WatchdogManager_Check(
        &manager,
        6u,
        &expired) == WATCHDOG_STATUS_CLIENT_EXPIRED);
}

static void TestBackendErrorsAreSeparated(void)
{
    FakeWatchdog fake = {0};
    WatchdogBackend backend;
    WatchdogManager manager;

    fake.init_status = WATCHDOG_STATUS_BACKEND_ERROR;
    backend = MakeBackend(&fake);
    assert(WatchdogManager_Init(
        &manager,
        &backend,
        100u,
        0u) == WATCHDOG_STATUS_BACKEND_ERROR);
}

int main(void)
{
    TestFeedRequiresHealthyClients();
    TestUnsignedTickWrap();
    TestBackendErrorsAreSeparated();

    puts("watchdog manager tests passed");
    return 0;
}
