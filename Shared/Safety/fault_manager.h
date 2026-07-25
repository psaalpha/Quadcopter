#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include <stdint.h>

typedef enum
{
    FAULT_ID_IMU = 0,
    FAULT_ID_COMMUNICATION_TIMEOUT,
    FAULT_ID_SENSOR,
    FAULT_ID_BATTERY,
    FAULT_ID_PARAMETER,
    FAULT_ID_WATCHDOG,
    FAULT_ID_COUNT
} FaultId;

typedef enum
{
    FAULT_LEVEL_NONE = 0,
    FAULT_LEVEL_INFO,
    FAULT_LEVEL_WARNING,
    FAULT_LEVEL_CRITICAL,
    FAULT_LEVEL_FATAL
} FaultLevel;

typedef enum
{
    FAULT_EVENT_RAISED = 0,
    FAULT_EVENT_UPDATED,
    FAULT_EVENT_CLEARED
} FaultEvent;

typedef struct
{
    FaultId id;
    FaultLevel level;
    int32_t argument;
    uint32_t first_timestamp_ms;
    uint32_t last_timestamp_ms;
    uint32_t occurrence_count;
    uint8_t active;
    uint8_t latched;
} FaultRecord;

typedef void (*FaultHandler)(
    void *context,
    FaultEvent event,
    const FaultRecord *record);

typedef struct
{
    FaultRecord records[FAULT_ID_COUNT];
    FaultHandler handler;
    void *handler_context;
    uint32_t active_mask;
    uint32_t latched_mask;
    uint8_t initialized;
} FaultManager;

void FaultManager_Init(
    FaultManager *manager,
    FaultHandler handler,
    void *handler_context);
uint8_t FaultManager_Raise(
    FaultManager *manager,
    FaultId id,
    FaultLevel level,
    uint8_t latch,
    int32_t argument,
    uint32_t timestamp_ms);
uint8_t FaultManager_Clear(
    FaultManager *manager,
    FaultId id,
    uint32_t timestamp_ms);
const FaultRecord *FaultManager_Get(
    const FaultManager *manager,
    FaultId id);
FaultLevel FaultManager_HighestActiveLevel(const FaultManager *manager);
uint32_t FaultManager_ActiveMask(const FaultManager *manager);
uint32_t FaultManager_LatchedMask(const FaultManager *manager);
void FaultManager_ResetHistory(FaultManager *manager);

#endif
