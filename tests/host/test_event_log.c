#include "event_log.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

typedef struct
{
    uint32_t enter_count;
    uint32_t exit_count;
    uint32_t depth;
} FakeLock;

typedef struct
{
    EventLogRecord records[8];
    uint16_t count;
    uint16_t fail_after;
} FakeSink;

static void FakeLock_Enter(void *context)
{
    FakeLock *lock = (FakeLock *)context;

    lock->enter_count++;
    lock->depth++;
}

static void FakeLock_Exit(void *context)
{
    FakeLock *lock = (FakeLock *)context;

    assert(lock->depth > 0u);
    lock->exit_count++;
    lock->depth--;
}

static EventLogStatus FakeSink_Write(
    void *context,
    const EventLogRecord *record)
{
    FakeSink *sink = (FakeSink *)context;

    if (sink->count >= sink->fail_after)
    {
        return EVENT_LOG_STATUS_SINK_ERROR;
    }

    sink->records[sink->count] = *record;
    sink->count++;
    return EVENT_LOG_STATUS_OK;
}

static EventLogRecord MakeRecord(uint16_t event_id)
{
    EventLogRecord record;

    record.timestamp_ms = (uint32_t)event_id * 10u;
    record.argument = (int32_t)event_id * -2;
    record.module_id = 3u;
    record.event_id = event_id;
    record.level = (uint8_t)EVENT_LOG_LEVEL_INFO;
    record.flags = 0u;
    record.reserved = 0u;
    return record;
}

static void TestFifoWrapAndOverflow(void)
{
    EventLogRecord storage[3];
    EventLogRecord record;
    EventLog log;

    assert(sizeof(EventLogRecord) == 16u);
    assert(EventLog_Init(&log, storage, 3u, 0) == EVENT_LOG_STATUS_OK);

    record = MakeRecord(1u);
    assert(EventLog_Push(&log, &record) == EVENT_LOG_STATUS_OK);
    record = MakeRecord(2u);
    assert(EventLog_Push(&log, &record) == EVENT_LOG_STATUS_OK);
    record = MakeRecord(3u);
    assert(EventLog_Push(&log, &record) == EVENT_LOG_STATUS_OK);
    record = MakeRecord(4u);
    assert(EventLog_Push(&log, &record) == EVENT_LOG_STATUS_FULL);
    assert(EventLog_Dropped(&log) == 1u);

    assert(EventLog_Pop(&log, &record) == EVENT_LOG_STATUS_OK);
    assert(record.event_id == 1u);
    assert(EventLog_Pop(&log, &record) == EVENT_LOG_STATUS_OK);
    assert(record.event_id == 2u);

    record = MakeRecord(4u);
    assert(EventLog_Push(&log, &record) == EVENT_LOG_STATUS_OK);
    record = MakeRecord(5u);
    assert(EventLog_Push(&log, &record) == EVENT_LOG_STATUS_OK);

    assert(EventLog_Pop(&log, &record) == EVENT_LOG_STATUS_OK);
    assert(record.event_id == 3u);
    assert(EventLog_Pop(&log, &record) == EVENT_LOG_STATUS_OK);
    assert(record.event_id == 4u);
    assert(EventLog_Pop(&log, &record) == EVENT_LOG_STATUS_OK);
    assert(record.event_id == 5u);
    assert(EventLog_Pop(&log, &record) == EVENT_LOG_STATUS_EMPTY);
}

static void TestLockAndDroppedCounterSaturation(void)
{
    EventLogRecord storage[1];
    EventLogRecord record;
    EventLog log;
    FakeLock fake_lock = {0};
    EventLogLock lock;

    lock.enter = FakeLock_Enter;
    lock.exit = FakeLock_Exit;
    lock.context = &fake_lock;

    assert(EventLog_Init(
        &log,
        storage,
        1u,
        &lock) == EVENT_LOG_STATUS_OK);
    assert(EventLog_Write(
        &log,
        10u,
        EVENT_LOG_LEVEL_WARNING,
        1u,
        2u,
        -3) == EVENT_LOG_STATUS_OK);

    log.dropped_count = UINT32_MAX;
    record = MakeRecord(2u);
    assert(EventLog_Push(&log, &record) == EVENT_LOG_STATUS_FULL);
    assert(EventLog_Dropped(&log) == UINT32_MAX);
    assert(fake_lock.enter_count == fake_lock.exit_count);
    assert(fake_lock.depth == 0u);
}

static void TestDrainStopsWithoutLosingFailedRecord(void)
{
    EventLogRecord storage[4];
    EventLogRecord record;
    EventLog log;
    FakeSink fake_sink = {0};
    EventLogSink sink;

    assert(EventLog_Init(&log, storage, 4u, 0) == EVENT_LOG_STATUS_OK);
    assert(EventLog_Write(
        &log,
        10u,
        EVENT_LOG_LEVEL_INFO,
        1u,
        10u,
        100) == EVENT_LOG_STATUS_OK);
    assert(EventLog_Write(
        &log,
        20u,
        EVENT_LOG_LEVEL_ERROR,
        1u,
        20u,
        200) == EVENT_LOG_STATUS_OK);
    assert(EventLog_Write(
        &log,
        30u,
        EVENT_LOG_LEVEL_FATAL,
        1u,
        30u,
        300) == EVENT_LOG_STATUS_OK);

    fake_sink.fail_after = 2u;
    sink.write = FakeSink_Write;
    sink.context = &fake_sink;

    assert(EventLog_Drain(&log, &sink, 4u) == 2u);
    assert(fake_sink.count == 2u);
    assert(EventLog_Pending(&log) == 1u);
    assert(EventLog_Peek(&log, &record) == EVENT_LOG_STATUS_OK);
    assert(record.event_id == 30u);
}

static void TestValidationAndClear(void)
{
    EventLogRecord storage[2];
    EventLogRecord record;
    EventLog log;

    assert(EventLog_Init(&log, storage, 2u, 0) == EVENT_LOG_STATUS_OK);
    record = MakeRecord(1u);
    record.level = 255u;
    assert(EventLog_Push(
        &log,
        &record) == EVENT_LOG_STATUS_INVALID_ARGUMENT);

    record = MakeRecord(2u);
    assert(EventLog_Push(&log, &record) == EVENT_LOG_STATUS_OK);
    assert(EventLog_Clear(&log) == EVENT_LOG_STATUS_OK);
    assert(EventLog_Pending(&log) == 0u);
}

int main(void)
{
    TestFifoWrapAndOverflow();
    TestLockAndDroppedCounterSaturation();
    TestDrainStopsWithoutLosingFailedRecord();
    TestValidationAndClear();

    puts("event log tests passed");
    return 0;
}
