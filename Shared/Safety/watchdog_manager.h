/**
 * @file watchdog_manager.h
 * @brief Heartbeat-gated hardware watchdog supervision and reset recording.
 *
 * The manager is hardware-independent. Platform code supplies the backend;
 * application clients report completed work through heartbeat calls.
 */
#ifndef WATCHDOG_MANAGER_H
#define WATCHDOG_MANAGER_H

#include <stdint.h>

#define WATCHDOG_MANAGER_MAX_CLIENTS  8u

typedef enum
{
    WATCHDOG_STATUS_OK = 0,
    WATCHDOG_STATUS_INVALID_ARGUMENT,
    WATCHDOG_STATUS_NOT_INITIALIZED,
    WATCHDOG_STATUS_BACKEND_ERROR,
    WATCHDOG_STATUS_CLIENT_NOT_REGISTERED,
    WATCHDOG_STATUS_CLIENT_NOT_READY,
    WATCHDOG_STATUS_CLIENT_EXPIRED
} WatchdogStatus;

typedef enum
{
    WATCHDOG_RESET_REASON_UNKNOWN = 0x00000001u,
    WATCHDOG_RESET_REASON_POWER_ON = 0x00000002u,
    WATCHDOG_RESET_REASON_PIN = 0x00000004u,
    WATCHDOG_RESET_REASON_SOFTWARE = 0x00000008u,
    WATCHDOG_RESET_REASON_INDEPENDENT_WATCHDOG = 0x00000010u,
    WATCHDOG_RESET_REASON_WINDOW_WATCHDOG = 0x00000020u,
    WATCHDOG_RESET_REASON_LOW_POWER = 0x00000040u
} WatchdogResetReason;

typedef WatchdogStatus (*WatchdogBackendInitFn)(
    void *context,
    uint32_t timeout_ms);
typedef WatchdogStatus (*WatchdogBackendFeedFn)(void *context);
typedef uint32_t (*WatchdogBackendReadResetFn)(void *context);
typedef void (*WatchdogBackendClearResetFn)(void *context);

typedef struct
{
    WatchdogBackendInitFn init;
    WatchdogBackendFeedFn feed;
    WatchdogBackendReadResetFn read_reset_reasons;
    WatchdogBackendClearResetFn clear_reset_reasons;
    void *context;
} WatchdogBackend;

typedef struct
{
    uint32_t reason_mask;
    uint32_t captured_at_ms;
    uint8_t valid;
} WatchdogResetRecord;

typedef struct
{
    WatchdogBackend backend;
    WatchdogResetRecord reset_record;
    uint32_t client_timeout_ms[WATCHDOG_MANAGER_MAX_CLIENTS];
    uint32_t client_last_heartbeat_ms[WATCHDOG_MANAGER_MAX_CLIENTS];
    uint32_t hardware_timeout_ms;
    uint32_t feed_count;
    uint32_t denied_feed_count;
    uint8_t registered_mask;
    uint8_t required_mask;
    uint8_t seen_mask;
    uint8_t expired_mask;
    uint8_t initialized;
} WatchdogManager;

WatchdogStatus WatchdogManager_Init(
    WatchdogManager *manager,
    const WatchdogBackend *backend,
    uint32_t hardware_timeout_ms,
    uint32_t timestamp_ms);
WatchdogStatus WatchdogManager_RegisterClient(
    WatchdogManager *manager,
    uint8_t client_id,
    uint32_t timeout_ms,
    uint8_t required);
WatchdogStatus WatchdogManager_Heartbeat(
    WatchdogManager *manager,
    uint8_t client_id,
    uint32_t timestamp_ms);
WatchdogStatus WatchdogManager_Check(
    WatchdogManager *manager,
    uint32_t timestamp_ms,
    uint8_t *expired_mask);
/**
 * @brief Feed the hardware watchdog only when every required client is healthy.
 * @param manager Initialized manager instance.
 * @param timestamp_ms Monotonic millisecond timestamp; unsigned wrap is valid.
 * @return WATCHDOG_STATUS_OK on a hardware feed, otherwise the gating reason.
 */
WatchdogStatus WatchdogManager_Feed(
    WatchdogManager *manager,
    uint32_t timestamp_ms);
const WatchdogResetRecord *WatchdogManager_GetResetRecord(
    const WatchdogManager *manager);

#endif
