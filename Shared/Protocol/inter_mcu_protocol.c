#include "inter_mcu_protocol.h"

#define INTER_MCU_CRC_OFFSET  39u

static void WriteU16Le(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xFFu);
    destination[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void WriteU32Le(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & 0xFFu);
    destination[1] = (uint8_t)((value >> 8) & 0xFFu);
    destination[2] = (uint8_t)((value >> 16) & 0xFFu);
    destination[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static uint16_t ReadU16Le(const uint8_t *source)
{
    return (uint16_t)((uint16_t)source[0] |
                      ((uint16_t)source[1] << 8));
}

static uint32_t ReadU32Le(const uint8_t *source)
{
    return (uint32_t)source[0] |
           ((uint32_t)source[1] << 8) |
           ((uint32_t)source[2] << 16) |
           ((uint32_t)source[3] << 24);
}

uint16_t InterMcu_Crc16Ccitt(const uint8_t *data, uint16_t length)
{
    uint16_t crc;
    uint16_t index;
    uint8_t bit;

    if (data == 0)
    {
        return 0u;
    }

    crc = 0xFFFFu;
    for (index = 0u; index < length; ++index)
    {
        crc ^= (uint16_t)((uint16_t)data[index] << 8);
        for (bit = 0u; bit < 8u; ++bit)
        {
            if ((crc & 0x8000u) != 0u)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

uint8_t InterMcu_EncodeSensorFrame(const InterMcuSensorData *data,
                                  uint8_t *frame,
                                  uint16_t frame_capacity)
{
    uint16_t crc;

    if ((data == 0) || (frame == 0) ||
        (frame_capacity < INTER_MCU_FRAME_SIZE))
    {
        return 0u;
    }

    frame[0] = INTER_MCU_MAGIC_0;
    frame[1] = INTER_MCU_MAGIC_1;
    frame[2] = INTER_MCU_PROTOCOL_VERSION;
    frame[3] = INTER_MCU_MESSAGE_SENSOR_DATA;
    frame[4] = INTER_MCU_SENSOR_PAYLOAD_SIZE;
    WriteU16Le(&frame[5], data->sequence);

    WriteU16Le(&frame[7], data->flags);
    WriteU32Le(&frame[9], data->timestamp_ms);
    WriteU32Le(&frame[13], (uint32_t)data->pressure_pa);
    WriteU16Le(&frame[17], (uint16_t)data->temperature_centi_c);
    WriteU32Le(&frame[19], (uint32_t)data->baro_altitude_mm);
    WriteU16Le(&frame[23], data->yaw_centi_deg);
    WriteU32Le(&frame[25], (uint32_t)data->flow_x);
    WriteU32Le(&frame[29], (uint32_t)data->flow_y);
    WriteU16Le(&frame[33], data->flow_distance_mm);
    frame[35] = data->flow_quality;
    WriteU16Le(&frame[36], data->battery_mv);
    frame[38] = 0u;

    crc = InterMcu_Crc16Ccitt(frame, INTER_MCU_CRC_OFFSET);
    WriteU16Le(&frame[INTER_MCU_CRC_OFFSET], crc);

    return 1u;
}

InterMcuDecodeStatus InterMcu_DecodeSensorFrame(
    const uint8_t *frame,
    uint16_t frame_length,
    InterMcuSensorData *data)
{
    uint16_t expected_crc;
    uint16_t actual_crc;

    if ((frame == 0) || (data == 0))
    {
        return INTER_MCU_DECODE_NULL_ARGUMENT;
    }
    if (frame_length != INTER_MCU_FRAME_SIZE)
    {
        return INTER_MCU_DECODE_FRAME_SIZE;
    }
    if ((frame[0] != INTER_MCU_MAGIC_0) ||
        (frame[1] != INTER_MCU_MAGIC_1))
    {
        return INTER_MCU_DECODE_MAGIC;
    }
    if (frame[2] != INTER_MCU_PROTOCOL_VERSION)
    {
        return INTER_MCU_DECODE_VERSION;
    }
    if (frame[3] != INTER_MCU_MESSAGE_SENSOR_DATA)
    {
        return INTER_MCU_DECODE_MESSAGE_TYPE;
    }
    if (frame[4] != INTER_MCU_SENSOR_PAYLOAD_SIZE)
    {
        return INTER_MCU_DECODE_PAYLOAD_SIZE;
    }

    expected_crc = ReadU16Le(&frame[INTER_MCU_CRC_OFFSET]);
    actual_crc = InterMcu_Crc16Ccitt(frame, INTER_MCU_CRC_OFFSET);
    if (actual_crc != expected_crc)
    {
        return INTER_MCU_DECODE_CRC;
    }

    data->sequence = ReadU16Le(&frame[5]);
    data->flags = ReadU16Le(&frame[7]);
    data->timestamp_ms = ReadU32Le(&frame[9]);
    data->pressure_pa = (int32_t)ReadU32Le(&frame[13]);
    data->temperature_centi_c = (int16_t)ReadU16Le(&frame[17]);
    data->baro_altitude_mm = (int32_t)ReadU32Le(&frame[19]);
    data->yaw_centi_deg = ReadU16Le(&frame[23]);
    data->flow_x = (int32_t)ReadU32Le(&frame[25]);
    data->flow_y = (int32_t)ReadU32Le(&frame[29]);
    data->flow_distance_mm = ReadU16Le(&frame[33]);
    data->flow_quality = frame[35];
    data->battery_mv = ReadU16Le(&frame[36]);

    return INTER_MCU_DECODE_OK;
}
