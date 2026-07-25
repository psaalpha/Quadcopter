#include "flight_data_logger.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    FlightDataRecord records[4];
    uint16_t count;
    uint16_t fail_after;
} FakeFlightSink;

static FlightDataLoggerStatus FakeFlightSink_Write(
    void *context,
    const FlightDataRecord *record)
{
    FakeFlightSink *sink = (FakeFlightSink *)context;

    if (sink->count >= sink->fail_after)
    {
        return FLIGHT_DATA_LOGGER_STATUS_SINK_ERROR;
    }
    sink->records[sink->count] = *record;
    sink->count++;
    return FLIGHT_DATA_LOGGER_STATUS_OK;
}

static FlightDataSnapshot MakeSnapshot(uint32_t timestamp_ms)
{
    FlightDataSnapshot snapshot;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.timestamp_ms = timestamp_ms;
    snapshot.attitude_deg[0] = 1.0f;
    snapshot.attitude_deg[1] = 2.0f;
    snapshot.attitude_deg[2] = 3.0f;
    snapshot.angular_rate_dps[0] = 10.0f;
    snapshot.pid_output[0] = -5.0f;
    snapshot.motor_output_ticks[0] = 500u;
    snapshot.motor_output_ticks[1] = 600u;
    snapshot.motor_output_ticks[2] = 700u;
    snapshot.motor_output_ticks[3] = 800u;
    snapshot.battery_mv = 11800u;
    snapshot.battery_current_ma = 1500;
    snapshot.fault_mask = 0x12u;
    snapshot.system_state = 2u;
    return snapshot;
}

static void TestFixedRecordAndDecimation(void)
{
    FlightDataRecord storage[2];
    FlightDataRecord record;
    FlightDataLogger logger;
    FlightDataSnapshot snapshot = MakeSnapshot(20u);

    assert(sizeof(FlightDataRecord) == 60u);
    assert(FlightDataLogger_Init(
        &logger,
        storage,
        2u,
        2u,
        0) == FLIGHT_DATA_LOGGER_STATUS_OK);

    assert(FlightDataLogger_Capture(
        &logger,
        &snapshot) == FLIGHT_DATA_LOGGER_STATUS_SKIPPED);
    assert(FlightDataLogger_Capture(
        &logger,
        &snapshot) == FLIGHT_DATA_LOGGER_STATUS_OK);
    assert(FlightDataLogger_Pending(&logger) == 1u);

    assert(FlightDataLogger_Pop(
        &logger,
        &record) == FLIGHT_DATA_LOGGER_STATUS_OK);
    assert(record.timestamp_ms == 20u);
    assert(record.attitude_deg[2] == 3.0f);
    assert(record.angular_rate_dps[0] == 10.0f);
    assert(record.pid_output[0] == -5.0f);
    assert(record.motor_output_ticks[3] == 800u);
    assert(record.battery_mv == 11800u);
    assert(record.fault_mask == 0x12u);
    assert(record.schema_version == FLIGHT_DATA_RECORD_SCHEMA_VERSION);
}

static void TestOverflowAndBoundedDrain(void)
{
    FlightDataRecord storage[2];
    FlightDataLogger logger;
    FlightDataSnapshot snapshot;
    FakeFlightSink fake_sink = {0};
    FlightDataLoggerSink sink;

    assert(FlightDataLogger_Init(
        &logger,
        storage,
        2u,
        1u,
        0) == FLIGHT_DATA_LOGGER_STATUS_OK);
    snapshot = MakeSnapshot(1u);
    assert(FlightDataLogger_Capture(
        &logger,
        &snapshot) == FLIGHT_DATA_LOGGER_STATUS_OK);
    snapshot.timestamp_ms = 2u;
    assert(FlightDataLogger_Capture(
        &logger,
        &snapshot) == FLIGHT_DATA_LOGGER_STATUS_OK);
    snapshot.timestamp_ms = 3u;
    assert(FlightDataLogger_Capture(
        &logger,
        &snapshot) == FLIGHT_DATA_LOGGER_STATUS_FULL);
    assert(FlightDataLogger_Dropped(&logger) == 1u);

    fake_sink.fail_after = 1u;
    sink.write = FakeFlightSink_Write;
    sink.context = &fake_sink;
    assert(FlightDataLogger_Drain(&logger, &sink, 2u) == 1u);
    assert(FlightDataLogger_Pending(&logger) == 1u);
    assert(fake_sink.records[0].timestamp_ms == 1u);
}

int main(void)
{
    TestFixedRecordAndDecimation();
    TestOverflowAndBoundedDrain();

    puts("flight data logger tests passed");
    return 0;
}
