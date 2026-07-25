#include "fault_manager.h"

#include <limits.h>

static uint8_t FaultManager_IdIsValid(FaultId id)
{
    return ((uint8_t)id < (uint8_t)FAULT_ID_COUNT) ? 1u : 0u;
}

static uint8_t FaultManager_LevelIsValid(FaultLevel level)
{
    return ((level >= FAULT_LEVEL_INFO) && (level <= FAULT_LEVEL_FATAL))
        ? 1u
        : 0u;
}

void FaultManager_Init(
    FaultManager *manager,
    FaultHandler handler,
    void *handler_context)
{
    uint8_t index;

    if (manager == 0)
    {
        return;
    }

    manager->handler = handler;
    manager->handler_context = handler_context;
    manager->active_mask = 0u;
    manager->latched_mask = 0u;
    manager->initialized = 1u;

    for (index = 0u; index < (uint8_t)FAULT_ID_COUNT; ++index)
    {
        manager->records[index].id = (FaultId)index;
        manager->records[index].level = FAULT_LEVEL_NONE;
        manager->records[index].argument = 0;
        manager->records[index].first_timestamp_ms = 0u;
        manager->records[index].last_timestamp_ms = 0u;
        manager->records[index].occurrence_count = 0u;
        manager->records[index].active = 0u;
        manager->records[index].latched = 0u;
    }
}

uint8_t FaultManager_Raise(
    FaultManager *manager,
    FaultId id,
    FaultLevel level,
    uint8_t latch,
    int32_t argument,
    uint32_t timestamp_ms)
{
    FaultRecord *record;
    FaultEvent event;
    uint32_t bit;

    if ((manager == 0) || !manager->initialized
        || !FaultManager_IdIsValid(id)
        || !FaultManager_LevelIsValid(level))
    {
        return 0u;
    }

    record = &manager->records[(uint8_t)id];
    event = record->active ? FAULT_EVENT_UPDATED : FAULT_EVENT_RAISED;
    if (!record->active)
    {
        record->first_timestamp_ms = timestamp_ms;
    }
    record->active = 1u;
    if (level > record->level)
    {
        record->level = level;
    }
    record->argument = argument;
    record->last_timestamp_ms = timestamp_ms;
    if (record->occurrence_count != UINT32_MAX)
    {
        record->occurrence_count++;
    }
    if (latch)
    {
        record->latched = 1u;
    }

    bit = (uint32_t)1u << (uint8_t)id;
    manager->active_mask |= bit;
    if (record->latched)
    {
        manager->latched_mask |= bit;
    }

    if (manager->handler != 0)
    {
        manager->handler(manager->handler_context, event, record);
    }
    return 1u;
}

uint8_t FaultManager_Clear(
    FaultManager *manager,
    FaultId id,
    uint32_t timestamp_ms)
{
    FaultRecord *record;
    uint32_t bit;

    if ((manager == 0) || !manager->initialized
        || !FaultManager_IdIsValid(id))
    {
        return 0u;
    }

    record = &manager->records[(uint8_t)id];
    if (!record->active)
    {
        return 1u;
    }

    record->active = 0u;
    record->level = FAULT_LEVEL_NONE;
    record->last_timestamp_ms = timestamp_ms;
    bit = (uint32_t)1u << (uint8_t)id;
    manager->active_mask &= ~bit;

    if (manager->handler != 0)
    {
        manager->handler(
            manager->handler_context,
            FAULT_EVENT_CLEARED,
            record);
    }
    return 1u;
}

const FaultRecord *FaultManager_Get(
    const FaultManager *manager,
    FaultId id)
{
    if ((manager == 0) || !manager->initialized
        || !FaultManager_IdIsValid(id))
    {
        return 0;
    }

    return &manager->records[(uint8_t)id];
}

FaultLevel FaultManager_HighestActiveLevel(const FaultManager *manager)
{
    uint8_t index;
    FaultLevel highest = FAULT_LEVEL_NONE;

    if ((manager == 0) || !manager->initialized)
    {
        return FAULT_LEVEL_NONE;
    }

    for (index = 0u; index < (uint8_t)FAULT_ID_COUNT; ++index)
    {
        if (manager->records[index].active
            && (manager->records[index].level > highest))
        {
            highest = manager->records[index].level;
        }
    }

    return highest;
}

uint32_t FaultManager_ActiveMask(const FaultManager *manager)
{
    return ((manager != 0) && manager->initialized)
        ? manager->active_mask
        : 0u;
}

uint32_t FaultManager_LatchedMask(const FaultManager *manager)
{
    return ((manager != 0) && manager->initialized)
        ? manager->latched_mask
        : 0u;
}

void FaultManager_ResetHistory(FaultManager *manager)
{
    uint8_t index;

    if ((manager == 0) || !manager->initialized)
    {
        return;
    }

    manager->latched_mask = 0u;
    for (index = 0u; index < (uint8_t)FAULT_ID_COUNT; ++index)
    {
        manager->records[index].latched = 0u;
        if (!manager->records[index].active)
        {
            manager->records[index].occurrence_count = 0u;
            manager->records[index].first_timestamp_ms = 0u;
            manager->records[index].last_timestamp_ms = 0u;
            manager->records[index].argument = 0;
        }
    }
}
