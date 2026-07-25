/**
 * @file flight_data_logger.h
 * @brief Bounded non-blocking capture and drain of flight-control snapshots.
 *
 * Capture uses fixed caller-owned storage and drops new records when full.
 * Slow sink work is performed only by the explicitly bounded drain operation.
 */
#ifndef FLIGHT_DATA_LOGGER_H
#define FLIGHT_DATA_LOGGER_H

#include <stdint.h>

#define FLIGHT_DATA_RECORD_SCHEMA_VERSION  1u

typedef enum
{
    FLIGHT_DATA_LOGGER_STATUS_OK = 0,
    FLIGHT_DATA_LOGGER_STATUS_SKIPPED,
    FLIGHT_DATA_LOGGER_STATUS_INVALID_ARGUMENT,
    FLIGHT_DATA_LOGGER_STATUS_NOT_INITIALIZED,
    FLIGHT_DATA_LOGGER_STATUS_EMPTY,
    FLIGHT_DATA_LOGGER_STATUS_FULL,
    FLIGHT_DATA_LOGGER_STATUS_SINK_ERROR
} FlightDataLoggerStatus;

typedef struct
{
    uint32_t timestamp_ms;
    float attitude_deg[3];
    float angular_rate_dps[3];
    float pid_output[3];
    uint16_t motor_output_ticks[4];
    uint16_t battery_mv;
    int16_t battery_current_ma;
    uint32_t fault_mask;
    uint8_t system_state;
    uint8_t flags;
} FlightDataSnapshot;

typedef struct
{
    uint32_t timestamp_ms;
    float attitude_deg[3];
    float angular_rate_dps[3];
    float pid_output[3];
    uint16_t motor_output_ticks[4];
    uint16_t battery_mv;
    int16_t battery_current_ma;
    uint32_t fault_mask;
    uint8_t system_state;
    uint8_t flags;
    uint16_t schema_version;
} FlightDataRecord;

typedef void (*FlightDataLoggerLockFn)(void *context);

typedef struct
{
    FlightDataLoggerLockFn enter;
    FlightDataLoggerLockFn exit;
    void *context;
} FlightDataLoggerLock;

typedef FlightDataLoggerStatus (*FlightDataLoggerSinkWriteFn)(
    void *context,
    const FlightDataRecord *record);

typedef struct
{
    FlightDataLoggerSinkWriteFn write;
    void *context;
} FlightDataLoggerSink;

typedef struct
{
    FlightDataRecord *storage;
    uint16_t capacity;
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    uint16_t capture_divider;
    uint16_t capture_counter;
    uint32_t captured_count;
    uint32_t dropped_count;
    FlightDataLoggerLock lock;
    uint8_t initialized;
} FlightDataLogger;

FlightDataLoggerStatus FlightDataLogger_Init(
    FlightDataLogger *logger,
    FlightDataRecord *storage,
    uint16_t capacity,
    uint16_t capture_divider,
    const FlightDataLoggerLock *lock);
/**
 * @brief Copy one snapshot into the ring when its capture divider is due.
 * @return OK, SKIPPED, FULL, or an initialization/argument error.
 */
FlightDataLoggerStatus FlightDataLogger_Capture(
    FlightDataLogger *logger,
    const FlightDataSnapshot *snapshot);
FlightDataLoggerStatus FlightDataLogger_Pop(
    FlightDataLogger *logger,
    FlightDataRecord *record);
uint16_t FlightDataLogger_Pending(FlightDataLogger *logger);
uint32_t FlightDataLogger_Dropped(FlightDataLogger *logger);
/**
 * @brief Send at most maximum_records to a non-blocking sink.
 * @return Number of records successfully consumed from the ring.
 */
uint16_t FlightDataLogger_Drain(
    FlightDataLogger *logger,
    const FlightDataLoggerSink *sink,
    uint16_t maximum_records);

#endif
