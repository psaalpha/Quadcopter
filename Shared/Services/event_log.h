#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include <stdint.h>

typedef enum
{
    EVENT_LOG_STATUS_OK = 0,
    EVENT_LOG_STATUS_INVALID_ARGUMENT,
    EVENT_LOG_STATUS_NOT_INITIALIZED,
    EVENT_LOG_STATUS_EMPTY,
    EVENT_LOG_STATUS_FULL,
    EVENT_LOG_STATUS_SINK_ERROR
} EventLogStatus;

typedef enum
{
    EVENT_LOG_LEVEL_DEBUG = 0,
    EVENT_LOG_LEVEL_INFO,
    EVENT_LOG_LEVEL_WARNING,
    EVENT_LOG_LEVEL_ERROR,
    EVENT_LOG_LEVEL_FATAL
} EventLogLevel;

typedef struct
{
    uint32_t timestamp_ms;
    int32_t argument;
    uint16_t module_id;
    uint16_t event_id;
    uint8_t level;
    uint8_t flags;
    uint16_t reserved;
} EventLogRecord;

typedef void (*EventLogLockFn)(void *context);

typedef struct
{
    EventLogLockFn enter;
    EventLogLockFn exit;
    void *context;
} EventLogLock;

typedef EventLogStatus (*EventLogSinkWriteFn)(
    void *context,
    const EventLogRecord *record);

typedef struct
{
    EventLogSinkWriteFn write;
    void *context;
} EventLogSink;

typedef struct
{
    EventLogRecord *storage;
    uint16_t capacity;
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    uint32_t dropped_count;
    EventLogLock lock;
    uint8_t initialized;
} EventLog;

EventLogStatus EventLog_Init(
    EventLog *log,
    EventLogRecord *storage,
    uint16_t capacity,
    const EventLogLock *lock);
EventLogStatus EventLog_Write(
    EventLog *log,
    uint32_t timestamp_ms,
    EventLogLevel level,
    uint16_t module_id,
    uint16_t event_id,
    int32_t argument);
EventLogStatus EventLog_Push(
    EventLog *log,
    const EventLogRecord *record);
EventLogStatus EventLog_Peek(
    EventLog *log,
    EventLogRecord *record);
EventLogStatus EventLog_Pop(
    EventLog *log,
    EventLogRecord *record);
uint16_t EventLog_Pending(EventLog *log);
uint32_t EventLog_Dropped(EventLog *log);
uint16_t EventLog_Drain(
    EventLog *log,
    const EventLogSink *sink,
    uint16_t maximum_records);
EventLogStatus EventLog_Clear(EventLog *log);

#endif
