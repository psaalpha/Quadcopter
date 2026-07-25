#include "inter_mcu_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void AssertRoundTrip(void)
{
    InterMcuSensorData source;
    InterMcuSensorData decoded;
    uint8_t frame[INTER_MCU_FRAME_SIZE];

    memset(&source, 0, sizeof(source));
    source.sequence = 0x1234u;
    source.flags = INTER_MCU_SENSOR_FLAG_BARO_VALID |
                   INTER_MCU_SENSOR_FLAG_FLOW_VALID;
    source.timestamp_ms = 0x12345678u;
    source.pressure_pa = 101325;
    source.temperature_centi_c = -1234;
    source.baro_altitude_mm = -2500;
    source.yaw_centi_deg = 35999u;
    source.flow_x = -1234567;
    source.flow_y = 7654321;
    source.flow_distance_mm = 4321u;
    source.flow_quality = 97u;
    source.battery_mv = 11850u;

    assert(InterMcu_EncodeSensorFrame(
               &source, frame, (uint16_t)sizeof(frame)) == 1u);
    assert(frame[0] == INTER_MCU_MAGIC_0);
    assert(frame[1] == INTER_MCU_MAGIC_1);
    assert(frame[2] == INTER_MCU_PROTOCOL_VERSION);
    assert(frame[3] == INTER_MCU_MESSAGE_SENSOR_DATA);
    assert(frame[4] == INTER_MCU_SENSOR_PAYLOAD_SIZE);
    assert(frame[5] == 0x34u);
    assert(frame[6] == 0x12u);
    assert(frame[9] == 0x78u);
    assert(frame[10] == 0x56u);
    assert(frame[11] == 0x34u);
    assert(frame[12] == 0x12u);

    memset(&decoded, 0, sizeof(decoded));
    assert(InterMcu_DecodeSensorFrame(
               frame,
               (uint16_t)sizeof(frame),
               &decoded) == INTER_MCU_DECODE_OK);
    assert(decoded.sequence == source.sequence);
    assert(decoded.flags == source.flags);
    assert(decoded.timestamp_ms == source.timestamp_ms);
    assert(decoded.pressure_pa == source.pressure_pa);
    assert(decoded.temperature_centi_c == source.temperature_centi_c);
    assert(decoded.baro_altitude_mm == source.baro_altitude_mm);
    assert(decoded.yaw_centi_deg == source.yaw_centi_deg);
    assert(decoded.flow_x == source.flow_x);
    assert(decoded.flow_y == source.flow_y);
    assert(decoded.flow_distance_mm == source.flow_distance_mm);
    assert(decoded.flow_quality == source.flow_quality);
    assert(decoded.battery_mv == source.battery_mv);
}

static void AssertValidationFailures(void)
{
    InterMcuSensorData data;
    uint8_t frame[INTER_MCU_FRAME_SIZE];

    memset(&data, 0, sizeof(data));
    assert(InterMcu_EncodeSensorFrame(
               &data, frame, (uint16_t)sizeof(frame)) == 1u);

    assert(InterMcu_DecodeSensorFrame(
               frame,
               INTER_MCU_FRAME_SIZE - 1u,
               &data) == INTER_MCU_DECODE_FRAME_SIZE);

    frame[0] ^= 0x01u;
    assert(InterMcu_DecodeSensorFrame(
               frame,
               (uint16_t)sizeof(frame),
               &data) == INTER_MCU_DECODE_MAGIC);
    frame[0] ^= 0x01u;

    frame[20] ^= 0x80u;
    assert(InterMcu_DecodeSensorFrame(
               frame,
               (uint16_t)sizeof(frame),
               &data) == INTER_MCU_DECODE_CRC);
}

static void AssertCrcReference(void)
{
    static const uint8_t reference[] = "123456789";

    assert(InterMcu_Crc16Ccitt(
               reference,
               (uint16_t)(sizeof(reference) - 1u)) == 0x29B1u);
}

int main(void)
{
    AssertCrcReference();
    AssertRoundTrip();
    AssertValidationFailures();

    puts("inter_mcu_protocol_test: PASS");
    return 0;
}
