#ifndef SHARED_PROTOCOL_INTER_MCU_PROTOCOL_H
#define SHARED_PROTOCOL_INTER_MCU_PROTOCOL_H

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
} inter_mcu_sensor_data_t;

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
} inter_mcu_decode_status_t;

uint16_t inter_mcu_crc16_ccitt(const uint8_t *data, uint16_t length);

uint8_t inter_mcu_encode_sensor_frame(const inter_mcu_sensor_data_t *data,
                                  uint8_t *frame,
                                  uint16_t frame_capacity);

inter_mcu_decode_status_t inter_mcu_decode_sensor_frame(
    const uint8_t *frame,
    uint16_t frame_length,
    inter_mcu_sensor_data_t *data);

#ifdef __cplusplus
}
#endif

#endif
