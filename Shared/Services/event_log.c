#include "event_log.h"

#include <limits.h>

static void EventLog_Enter(EventLog *log)
{
    if (log->lock.enter != 0)
    {
        log->lock.enter(log->lock.context);
    }
}

static void EventLog_Exit(EventLog *log)
{
    if (log->lock.exit != 0)
    {
        log->lock.exit(log->lock.context);
    }
}

static uint16_t EventLog_Advance(uint16_t index, uint16_t capacity)
{
    index++;
    return (index >= capacity) ? 0u : index;
}

static uint8_t EventLog_LevelIsValid(uint8_t level)
{
    return (level <= (uint8_t)EVENT_LOG_LEVEL_FATAL)
        ? 1u
        : 0u;
}

EventLogStatus EventLog_Init(
    EventLog *log,
    EventLogRecord *storage,
    uint16_t capacity,
    const EventLogLock *lock)
{
    if ((log == 0) || (storage == 0) || (capacity == 0u))
    {
        return EVENT_LOG_STATUS_INVALID_ARGUMENT;
    }
    if ((lock != 0) && ((lock->enter == 0) != (lock->exit == 0)))
    {
        return EVENT_LOG_STATUS_INVALID_ARGUMENT;
    }

    log->storage = storage;
    log->capacity = capacity;
    log->head = 0u;
    log->tail = 0u;
    log->count = 0u;
    log->dropped_count = 0u;
    log->initialized = 1u;

    if (lock != 0)
    {
        log->lock = *lock;
    }
    else
    {
        log->lock.enter = 0;
        log->lock.exit = 0;
        log->lock.context = 0;
    }

    return EVENT_LOG_STATUS_OK;
}

EventLogStatus EventLog_Write(
    EventLog *log,
    uint32_t timestamp_ms,
    EventLogLevel level,
    uint16_t module_id,
    uint16_t event_id,
    int32_t argument)
{
    EventLogRecord record;

    record.timestamp_ms = timestamp_ms;
    record.argument = argument;
    record.module_id = module_id;
    record.event_id = event_id;
    record.level = (uint8_t)level;
    record.flags = 0u;
    record.reserved = 0u;
    return EventLog_Push(log, &record);
}

EventLogStatus EventLog_Push(
    EventLog *log,
    const EventLogRecord *record)
{
    if ((log == 0) || (record == 0))
    {
        return EVENT_LOG_STATUS_INVALID_ARGUMENT;
    }
    if (!log->initialized)
    {
        return EVENT_LOG_STATUS_NOT_INITIALIZED;
    }
    if (!EventLog_LevelIsValid(record->level))
    {
        return EVENT_LOG_STATUS_INVALID_ARGUMENT;
    }

    EventLog_Enter(log);
    if (log->count >= log->capacity)
    {
        if (log->dropped_count != UINT32_MAX)
        {
            log->dropped_count++;
        }
        EventLog_Exit(log);
        return EVENT_LOG_STATUS_FULL;
    }

    log->storage[log->head] = *record;
    log->head = EventLog_Advance(log->head, log->capacity);
    log->count++;
    EventLog_Exit(log);
    return EVENT_LOG_STATUS_OK;
}

EventLogStatus EventLog_Peek(
    EventLog *log,
    EventLogRecord *record)
{
    if ((log == 0) || (record == 0))
    {
        return EVENT_LOG_STATUS_INVALID_ARGUMENT;
    }
    if (!log->initialized)
    {
        return EVENT_LOG_STATUS_NOT_INITIALIZED;
    }

    EventLog_Enter(log);
    if (log->count == 0u)
    {
        EventLog_Exit(log);
        return EVENT_LOG_STATUS_EMPTY;
    }

    *record = log->storage[log->tail];
    EventLog_Exit(log);
    return EVENT_LOG_STATUS_OK;
}

EventLogStatus EventLog_Pop(
    EventLog *log,
    EventLogRecord *record)
{
    if ((log == 0) || (record == 0))
    {
        return EVENT_LOG_STATUS_INVALID_ARGUMENT;
    }
    if (!log->initialized)
    {
        return EVENT_LOG_STATUS_NOT_INITIALIZED;
    }

    EventLog_Enter(log);
    if (log->count == 0u)
    {
        EventLog_Exit(log);
        return EVENT_LOG_STATUS_EMPTY;
    }

    *record = log->storage[log->tail];
    log->tail = EventLog_Advance(log->tail, log->capacity);
    log->count--;
    EventLog_Exit(log);
    return EVENT_LOG_STATUS_OK;
}

uint16_t EventLog_Pending(EventLog *log)
{
    uint16_t pending;

    if ((log == 0) || !log->initialized)
    {
        return 0u;
    }

    EventLog_Enter(log);
    pending = log->count;
    EventLog_Exit(log);
    return pending;
}

uint32_t EventLog_Dropped(EventLog *log)
{
    uint32_t dropped;

    if ((log == 0) || !log->initialized)
    {
        return 0u;
    }

    EventLog_Enter(log);
    dropped = log->dropped_count;
    EventLog_Exit(log);
    return dropped;
}

uint16_t EventLog_Drain(
    EventLog *log,
    const EventLogSink *sink,
    uint16_t maximum_records)
{
    EventLogRecord record;
    uint16_t drained = 0u;

    if ((log == 0) || (sink == 0) || (sink->write == 0)
        || !log->initialized)
    {
        return 0u;
    }

    while (drained < maximum_records)
    {
        if (EventLog_Peek(log, &record) != EVENT_LOG_STATUS_OK)
        {
            break;
        }
        if (sink->write(sink->context, &record) != EVENT_LOG_STATUS_OK)
        {
            break;
        }
        if (EventLog_Pop(log, &record) != EVENT_LOG_STATUS_OK)
        {
            break;
        }
        drained++;
    }

    return drained;
}

EventLogStatus EventLog_Clear(EventLog *log)
{
    if (log == 0)
    {
        return EVENT_LOG_STATUS_INVALID_ARGUMENT;
    }
    if (!log->initialized)
    {
        return EVENT_LOG_STATUS_NOT_INITIALIZED;
    }

    EventLog_Enter(log);
    log->head = 0u;
    log->tail = 0u;
    log->count = 0u;
    EventLog_Exit(log);
    return EVENT_LOG_STATUS_OK;
}
