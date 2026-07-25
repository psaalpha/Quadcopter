#include "flight_data_logger.h"

#include <limits.h>

static void FlightDataLogger_Enter(FlightDataLogger *logger)
{
    if (logger->lock.enter != 0)
    {
        logger->lock.enter(logger->lock.context);
    }
}

static void FlightDataLogger_Exit(FlightDataLogger *logger)
{
    if (logger->lock.exit != 0)
    {
        logger->lock.exit(logger->lock.context);
    }
}

static uint16_t FlightDataLogger_Advance(
    uint16_t index,
    uint16_t capacity)
{
    index++;
    return (index >= capacity) ? 0u : index;
}

static void FlightDataLogger_Increment(uint32_t *counter)
{
    if (*counter != UINT32_MAX)
    {
        (*counter)++;
    }
}

static void FlightDataLogger_CopyRecord(
    FlightDataRecord *record,
    const FlightDataSnapshot *snapshot)
{
    uint8_t index;

    record->timestamp_ms = snapshot->timestamp_ms;
    for (index = 0u; index < 3u; ++index)
    {
        record->attitude_deg[index] = snapshot->attitude_deg[index];
        record->angular_rate_dps[index] = snapshot->angular_rate_dps[index];
        record->pid_output[index] = snapshot->pid_output[index];
    }
    for (index = 0u; index < 4u; ++index)
    {
        record->motor_output_ticks[index] =
            snapshot->motor_output_ticks[index];
    }
    record->battery_mv = snapshot->battery_mv;
    record->battery_current_ma = snapshot->battery_current_ma;
    record->fault_mask = snapshot->fault_mask;
    record->system_state = snapshot->system_state;
    record->flags = snapshot->flags;
    record->schema_version = FLIGHT_DATA_RECORD_SCHEMA_VERSION;
}

FlightDataLoggerStatus FlightDataLogger_Init(
    FlightDataLogger *logger,
    FlightDataRecord *storage,
    uint16_t capacity,
    uint16_t capture_divider,
    const FlightDataLoggerLock *lock)
{
    if ((logger == 0) || (storage == 0)
        || (capacity == 0u) || (capture_divider == 0u))
    {
        return FLIGHT_DATA_LOGGER_STATUS_INVALID_ARGUMENT;
    }
    if ((lock != 0) && ((lock->enter == 0) != (lock->exit == 0)))
    {
        return FLIGHT_DATA_LOGGER_STATUS_INVALID_ARGUMENT;
    }

    logger->storage = storage;
    logger->capacity = capacity;
    logger->head = 0u;
    logger->tail = 0u;
    logger->count = 0u;
    logger->capture_divider = capture_divider;
    logger->capture_counter = 0u;
    logger->captured_count = 0u;
    logger->dropped_count = 0u;
    logger->initialized = 1u;

    if (lock != 0)
    {
        logger->lock = *lock;
    }
    else
    {
        logger->lock.enter = 0;
        logger->lock.exit = 0;
        logger->lock.context = 0;
    }
    return FLIGHT_DATA_LOGGER_STATUS_OK;
}

FlightDataLoggerStatus FlightDataLogger_Capture(
    FlightDataLogger *logger,
    const FlightDataSnapshot *snapshot)
{
    if ((logger == 0) || (snapshot == 0))
    {
        return FLIGHT_DATA_LOGGER_STATUS_INVALID_ARGUMENT;
    }
    if (!logger->initialized)
    {
        return FLIGHT_DATA_LOGGER_STATUS_NOT_INITIALIZED;
    }

    logger->capture_counter++;
    if (logger->capture_counter < logger->capture_divider)
    {
        return FLIGHT_DATA_LOGGER_STATUS_SKIPPED;
    }
    logger->capture_counter = 0u;

    FlightDataLogger_Enter(logger);
    if (logger->count >= logger->capacity)
    {
        FlightDataLogger_Increment(&logger->dropped_count);
        FlightDataLogger_Exit(logger);
        return FLIGHT_DATA_LOGGER_STATUS_FULL;
    }

    FlightDataLogger_CopyRecord(
        &logger->storage[logger->head],
        snapshot);
    logger->head = FlightDataLogger_Advance(
        logger->head,
        logger->capacity);
    logger->count++;
    FlightDataLogger_Increment(&logger->captured_count);
    FlightDataLogger_Exit(logger);
    return FLIGHT_DATA_LOGGER_STATUS_OK;
}

FlightDataLoggerStatus FlightDataLogger_Pop(
    FlightDataLogger *logger,
    FlightDataRecord *record)
{
    if ((logger == 0) || (record == 0))
    {
        return FLIGHT_DATA_LOGGER_STATUS_INVALID_ARGUMENT;
    }
    if (!logger->initialized)
    {
        return FLIGHT_DATA_LOGGER_STATUS_NOT_INITIALIZED;
    }

    FlightDataLogger_Enter(logger);
    if (logger->count == 0u)
    {
        FlightDataLogger_Exit(logger);
        return FLIGHT_DATA_LOGGER_STATUS_EMPTY;
    }

    *record = logger->storage[logger->tail];
    logger->tail = FlightDataLogger_Advance(
        logger->tail,
        logger->capacity);
    logger->count--;
    FlightDataLogger_Exit(logger);
    return FLIGHT_DATA_LOGGER_STATUS_OK;
}

uint16_t FlightDataLogger_Pending(FlightDataLogger *logger)
{
    uint16_t count;

    if ((logger == 0) || !logger->initialized)
    {
        return 0u;
    }

    FlightDataLogger_Enter(logger);
    count = logger->count;
    FlightDataLogger_Exit(logger);
    return count;
}

uint32_t FlightDataLogger_Dropped(FlightDataLogger *logger)
{
    uint32_t count;

    if ((logger == 0) || !logger->initialized)
    {
        return 0u;
    }

    FlightDataLogger_Enter(logger);
    count = logger->dropped_count;
    FlightDataLogger_Exit(logger);
    return count;
}

uint16_t FlightDataLogger_Drain(
    FlightDataLogger *logger,
    const FlightDataLoggerSink *sink,
    uint16_t maximum_records)
{
    FlightDataRecord record;
    uint16_t drained = 0u;

    if ((logger == 0) || !logger->initialized
        || (sink == 0) || (sink->write == 0))
    {
        return 0u;
    }

    while (drained < maximum_records)
    {
        FlightDataLogger_Enter(logger);
        if (logger->count == 0u)
        {
            FlightDataLogger_Exit(logger);
            break;
        }
        record = logger->storage[logger->tail];
        FlightDataLogger_Exit(logger);

        if (sink->write(sink->context, &record)
            != FLIGHT_DATA_LOGGER_STATUS_OK)
        {
            break;
        }
        if (FlightDataLogger_Pop(logger, &record)
            != FLIGHT_DATA_LOGGER_STATUS_OK)
        {
            break;
        }
        drained++;
    }

    return drained;
}
