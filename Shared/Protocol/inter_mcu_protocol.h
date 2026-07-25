#ifndef INTER_MCU_PROTOCOL_H
#define INTER_MCU_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INTER_MCU_MAGIC_0              0xA5u
#define INTER_MCU_MAGIC_1              0x5Au
#define INTER_MCU_PROTOCOL_VERSION     1u
#define INTER_MCU_MESSAGE_SENSOR_DATA  1u
#define INTER_MCU_SENSOR_PAYLOAD_SIZE  32u
#define INTER_MCU_FRAME_SIZE           41u

#define INTER_MCU_SENSOR_FLAG_BARO_VALID       (1u << 0)
#define INTER_MCU_SENSOR_FLAG_MAG_VALID        (1u << 1)
#define INTER_MCU_SENSOR_FLAG_FLOW_VALID       (1u << 2)
#define INTER_MCU_SENSOR_FLAG_LOW_BATTERY      (1u << 3)
#define INTER_MCU_SENSOR_FLAG_MAG_CALIBRATING  (1u << 4)
#define INTER_MCU_SENSOR_FLAG_BATTERY_VALID    (1u << 5)

typedef struct
{
    uint16_t sequence;
    uint16_t flags;
    uint32_t timestamp_ms;
    int32_t  pressure_pa;
    int16_t  temperature_centi_c;
    int32_t  baro_altitude_mm;
    uint16_t yaw_centi_deg;
    int32_t  flow_x;
    int32_t  flow_y;
    uint16_t flow_distance_mm;
    uint8_t  flow_quality;
    uint16_t battery_mv;
} InterMcuSensorData;

typedef enum
{
    INTER_MCU_DECODE_OK = 0,
    INTER_MCU_DECODE_NULL_ARGUMENT,
    INTER_MCU_DECODE_FRAME_SIZE,
    INTER_MCU_DECODE_MAGIC,
    INTER_MCU_DECODE_VERSION,
    INTER_MCU_DECODE_MESSAGE_TYPE,
    INTER_MCU_DECODE_PAYLOAD_SIZE,
    INTER_MCU_DECODE_CRC
} InterMcuDecodeStatus;

uint16_t InterMcu_Crc16Ccitt(const uint8_t *data, uint16_t length);

uint8_t InterMcu_EncodeSensorFrame(const InterMcuSensorData *data,
                                  uint8_t *frame,
                                  uint16_t frame_capacity);

InterMcuDecodeStatus InterMcu_DecodeSensorFrame(
    const uint8_t *frame,
    uint16_t frame_length,
    InterMcuSensorData *data);

#ifdef __cplusplus
}
#endif

#endif
