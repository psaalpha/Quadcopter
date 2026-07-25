#include "watchdog_manager.h"

#include <limits.h>

static void WatchdogManager_Increment(uint32_t *counter)
{
    if (*counter != UINT32_MAX)
    {
        (*counter)++;
    }
}

WatchdogStatus WatchdogManager_Init(
    WatchdogManager *manager,
    const WatchdogBackend *backend,
    uint32_t hardware_timeout_ms,
    uint32_t timestamp_ms)
{
    uint8_t index;
    WatchdogStatus status;

    if ((manager == 0) || (backend == 0)
        || (backend->init == 0) || (backend->feed == 0)
        || (hardware_timeout_ms == 0u))
    {
        return WATCHDOG_STATUS_INVALID_ARGUMENT;
    }

    manager->backend = *backend;
    manager->hardware_timeout_ms = hardware_timeout_ms;
    manager->feed_count = 0u;
    manager->denied_feed_count = 0u;
    manager->registered_mask = 0u;
    manager->required_mask = 0u;
    manager->seen_mask = 0u;
    manager->expired_mask = 0u;
    manager->initialized = 0u;

    for (index = 0u; index < WATCHDOG_MANAGER_MAX_CLIENTS; ++index)
    {
        manager->client_timeout_ms[index] = 0u;
        manager->client_last_heartbeat_ms[index] = 0u;
    }

    manager->reset_record.reason_mask = WATCHDOG_RESET_REASON_UNKNOWN;
    manager->reset_record.captured_at_ms = timestamp_ms;
    manager->reset_record.valid = 1u;
    if (backend->read_reset_reasons != 0)
    {
        manager->reset_record.reason_mask =
            backend->read_reset_reasons(backend->context);
        if (manager->reset_record.reason_mask == 0u)
        {
            manager->reset_record.reason_mask =
                WATCHDOG_RESET_REASON_UNKNOWN;
        }
    }
    if (backend->clear_reset_reasons != 0)
    {
        backend->clear_reset_reasons(backend->context);
    }

    status = backend->init(backend->context, hardware_timeout_ms);
    if (status != WATCHDOG_STATUS_OK)
    {
        return WATCHDOG_STATUS_BACKEND_ERROR;
    }

    manager->initialized = 1u;
    return WATCHDOG_STATUS_OK;
}

WatchdogStatus WatchdogManager_RegisterClient(
    WatchdogManager *manager,
    uint8_t client_id,
    uint32_t timeout_ms,
    uint8_t required)
{
    uint8_t bit;

    if ((manager == 0) || (client_id >= WATCHDOG_MANAGER_MAX_CLIENTS)
        || (timeout_ms == 0u))
    {
        return WATCHDOG_STATUS_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return WATCHDOG_STATUS_NOT_INITIALIZED;
    }

    bit = (uint8_t)(1u << client_id);
    manager->client_timeout_ms[client_id] = timeout_ms;
    manager->registered_mask |= bit;
    manager->seen_mask &= (uint8_t)~bit;
    manager->expired_mask &= (uint8_t)~bit;
    if (required)
    {
        manager->required_mask |= bit;
    }
    else
    {
        manager->required_mask &= (uint8_t)~bit;
    }

    return WATCHDOG_STATUS_OK;
}

WatchdogStatus WatchdogManager_Heartbeat(
    WatchdogManager *manager,
    uint8_t client_id,
    uint32_t timestamp_ms)
{
    uint8_t bit;

    if ((manager == 0) || (client_id >= WATCHDOG_MANAGER_MAX_CLIENTS))
    {
        return WATCHDOG_STATUS_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return WATCHDOG_STATUS_NOT_INITIALIZED;
    }

    bit = (uint8_t)(1u << client_id);
    if ((manager->registered_mask & bit) == 0u)
    {
        return WATCHDOG_STATUS_CLIENT_NOT_REGISTERED;
    }

    manager->client_last_heartbeat_ms[client_id] = timestamp_ms;
    manager->seen_mask |= bit;
    manager->expired_mask &= (uint8_t)~bit;
    return WATCHDOG_STATUS_OK;
}

WatchdogStatus WatchdogManager_Check(
    WatchdogManager *manager,
    uint32_t timestamp_ms,
    uint8_t *expired_mask)
{
    uint8_t client_id;
    uint8_t bit;
    uint8_t expired = 0u;
    uint8_t missing;

    if ((manager == 0) || (expired_mask == 0))
    {
        return WATCHDOG_STATUS_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return WATCHDOG_STATUS_NOT_INITIALIZED;
    }

    missing = (uint8_t)(manager->required_mask
        & (uint8_t)~manager->seen_mask);
    for (client_id = 0u;
         client_id < WATCHDOG_MANAGER_MAX_CLIENTS;
         ++client_id)
    {
        bit = (uint8_t)(1u << client_id);
        if (((manager->required_mask & bit) != 0u)
            && ((manager->seen_mask & bit) != 0u)
            && ((uint32_t)(timestamp_ms
                    - manager->client_last_heartbeat_ms[client_id])
                > manager->client_timeout_ms[client_id]))
        {
            expired |= bit;
        }
    }

    manager->expired_mask = expired;
    *expired_mask = expired;
    if (missing != 0u)
    {
        return WATCHDOG_STATUS_CLIENT_NOT_READY;
    }
    if (expired != 0u)
    {
        return WATCHDOG_STATUS_CLIENT_EXPIRED;
    }

    return WATCHDOG_STATUS_OK;
}

WatchdogStatus WatchdogManager_Feed(
    WatchdogManager *manager,
    uint32_t timestamp_ms)
{
    uint8_t expired;
    WatchdogStatus status;

    if (manager == 0)
    {
        return WATCHDOG_STATUS_INVALID_ARGUMENT;
    }
    if (!manager->initialized)
    {
        return WATCHDOG_STATUS_NOT_INITIALIZED;
    }

    status = WatchdogManager_Check(manager, timestamp_ms, &expired);
    if (status != WATCHDOG_STATUS_OK)
    {
        WatchdogManager_Increment(&manager->denied_feed_count);
        return status;
    }

    status = manager->backend.feed(manager->backend.context);
    if (status != WATCHDOG_STATUS_OK)
    {
        return WATCHDOG_STATUS_BACKEND_ERROR;
    }

    WatchdogManager_Increment(&manager->feed_count);
    return WATCHDOG_STATUS_OK;
}

const WatchdogResetRecord *WatchdogManager_GetResetRecord(
    const WatchdogManager *manager)
{
    if ((manager == 0) || !manager->initialized)
    {
        return 0;
    }

    return &manager->reset_record;
}
