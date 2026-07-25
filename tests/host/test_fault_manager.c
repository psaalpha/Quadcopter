#include "fault_manager.h"

#include <assert.h>
#include <stdio.h>

typedef struct
{
    uint32_t raised;
    uint32_t updated;
    uint32_t cleared;
    FaultId last_id;
} HandlerTrace;

static void TraceHandler(
    void *context,
    FaultEvent event,
    const FaultRecord *record)
{
    HandlerTrace *trace = (HandlerTrace *)context;

    trace->last_id = record->id;
    if (event == FAULT_EVENT_RAISED)
    {
        trace->raised++;
    }
    else if (event == FAULT_EVENT_UPDATED)
    {
        trace->updated++;
    }
    else
    {
        trace->cleared++;
    }
}

static void TestRaiseUpdateAndClear(void)
{
    FaultManager manager;
    HandlerTrace trace = {0};
    const FaultRecord *record;

    FaultManager_Init(&manager, TraceHandler, &trace);
    assert(FaultManager_Raise(
        &manager,
        FAULT_ID_IMU,
        FAULT_LEVEL_WARNING,
        0u,
        12,
        100u) == 1u);
    assert(trace.raised == 1u);
    assert(FaultManager_HighestActiveLevel(&manager) == FAULT_LEVEL_WARNING);

    assert(FaultManager_Raise(
        &manager,
        FAULT_ID_IMU,
        FAULT_LEVEL_CRITICAL,
        1u,
        99,
        110u) == 1u);
    record = FaultManager_Get(&manager, FAULT_ID_IMU);
    assert(record != 0);
    assert(record->level == FAULT_LEVEL_CRITICAL);
    assert(record->occurrence_count == 2u);
    assert(record->first_timestamp_ms == 100u);
    assert(record->last_timestamp_ms == 110u);
    assert(record->argument == 99);
    assert(trace.updated == 1u);
    assert(FaultManager_LatchedMask(&manager) == 0x01u);

    assert(FaultManager_Clear(&manager, FAULT_ID_IMU, 120u) == 1u);
    assert(FaultManager_ActiveMask(&manager) == 0u);
    assert(FaultManager_LatchedMask(&manager) == 0x01u);
    assert(trace.cleared == 1u);
    assert(trace.last_id == FAULT_ID_IMU);

    FaultManager_ResetHistory(&manager);
    assert(FaultManager_LatchedMask(&manager) == 0u);
    assert(record->occurrence_count == 0u);
}

static void TestHighestActiveLevel(void)
{
    FaultManager manager;

    FaultManager_Init(&manager, 0, 0);
    assert(FaultManager_Raise(
        &manager,
        FAULT_ID_BATTERY,
        FAULT_LEVEL_WARNING,
        0u,
        3300,
        1u) == 1u);
    assert(FaultManager_Raise(
        &manager,
        FAULT_ID_PARAMETER,
        FAULT_LEVEL_FATAL,
        1u,
        -1,
        2u) == 1u);

    assert(FaultManager_HighestActiveLevel(&manager) == FAULT_LEVEL_FATAL);
    assert(FaultManager_ActiveMask(&manager)
        == ((1u << FAULT_ID_BATTERY) | (1u << FAULT_ID_PARAMETER)));
}

int main(void)
{
    TestRaiseUpdateAndClear();
    TestHighestActiveLevel();

    puts("fault manager tests passed");
    return 0;
}
