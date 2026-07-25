#include "ground_station_protocol.h"

#include <string.h>

#define GROUND_STATION_MAGIC_0  0x51u
#define GROUND_STATION_MAGIC_1  0x47u

static uint16_t GroundStationProtocol_ReadU16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0]
        | ((uint16_t)data[1] << 8));
}

static void GroundStationProtocol_WriteU16(
    uint8_t *data,
    uint16_t value)
{
    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)((value >> 8) & 0xFFu);
}

uint16_t GroundStationProtocol_Crc16(
    const uint8_t *data,
    uint16_t length)
{
    uint16_t crc = 0xFFFFu;
    uint16_t index;
    uint8_t bit;

    if ((data == 0) && (length != 0u))
    {
        return 0u;
    }

    for (index = 0u; index < length; ++index)
    {
        crc ^= (uint16_t)data[index] << 8;
        for (bit = 0u; bit < 8u; ++bit)
        {
            crc = (crc & 0x8000u)
                ? (uint16_t)((crc << 1) ^ 0x1021u)
                : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

GroundStationProtocolStatus GroundStationProtocol_Encode(
    const GroundStationFrame *frame,
    uint8_t *output,
    uint16_t capacity,
    uint16_t *written_size)
{
    uint16_t frame_size;
    uint16_t crc;

    if ((frame == 0) || (output == 0) || (written_size == 0))
    {
        return GROUND_STATION_PROTOCOL_INVALID_ARGUMENT;
    }
    if (frame->payload_length > GROUND_STATION_MAX_PAYLOAD_SIZE)
    {
        return GROUND_STATION_PROTOCOL_BAD_LENGTH;
    }

    frame_size = (uint16_t)(GROUND_STATION_FRAME_HEADER_SIZE
        + frame->payload_length + GROUND_STATION_FRAME_CRC_SIZE);
    if (capacity < frame_size)
    {
        return GROUND_STATION_PROTOCOL_BUFFER_TOO_SMALL;
    }

    output[0] = GROUND_STATION_MAGIC_0;
    output[1] = GROUND_STATION_MAGIC_1;
    output[2] = GROUND_STATION_PROTOCOL_VERSION;
    output[3] = (uint8_t)frame->message_type;
    output[4] = frame->flags;
    output[5] = 0u;
    GroundStationProtocol_WriteU16(&output[6], frame->sequence);
    GroundStationProtocol_WriteU16(&output[8], frame->payload_length);
    if (frame->payload_length != 0u)
    {
        memcpy(
            &output[GROUND_STATION_FRAME_HEADER_SIZE],
            frame->payload,
            frame->payload_length);
    }

    crc = GroundStationProtocol_Crc16(
        output,
        (uint16_t)(GROUND_STATION_FRAME_HEADER_SIZE
            + frame->payload_length));
    GroundStationProtocol_WriteU16(
        &output[GROUND_STATION_FRAME_HEADER_SIZE
            + frame->payload_length],
        crc);
    *written_size = frame_size;
    return GROUND_STATION_PROTOCOL_OK;
}

GroundStationProtocolStatus GroundStationProtocol_Decode(
    GroundStationFrame *frame,
    const uint8_t *input,
    uint16_t input_size)
{
    uint16_t payload_length;
    uint16_t expected_size;
    uint16_t expected_crc;
    uint16_t actual_crc;

    if ((frame == 0) || (input == 0))
    {
        return GROUND_STATION_PROTOCOL_INVALID_ARGUMENT;
    }
    if (input_size < (GROUND_STATION_FRAME_HEADER_SIZE
        + GROUND_STATION_FRAME_CRC_SIZE))
    {
        return GROUND_STATION_PROTOCOL_BAD_LENGTH;
    }
    if ((input[0] != GROUND_STATION_MAGIC_0)
        || (input[1] != GROUND_STATION_MAGIC_1))
    {
        return GROUND_STATION_PROTOCOL_BAD_MAGIC;
    }
    if (input[2] != GROUND_STATION_PROTOCOL_VERSION)
    {
        return GROUND_STATION_PROTOCOL_UNSUPPORTED_VERSION;
    }

    payload_length = GroundStationProtocol_ReadU16(&input[8]);
    if (payload_length > GROUND_STATION_MAX_PAYLOAD_SIZE)
    {
        return GROUND_STATION_PROTOCOL_BAD_LENGTH;
    }
    expected_size = (uint16_t)(GROUND_STATION_FRAME_HEADER_SIZE
        + payload_length + GROUND_STATION_FRAME_CRC_SIZE);
    if (input_size != expected_size)
    {
        return GROUND_STATION_PROTOCOL_BAD_LENGTH;
    }

    expected_crc = GroundStationProtocol_ReadU16(
        &input[GROUND_STATION_FRAME_HEADER_SIZE + payload_length]);
    actual_crc = GroundStationProtocol_Crc16(
        input,
        (uint16_t)(GROUND_STATION_FRAME_HEADER_SIZE + payload_length));
    if (actual_crc != expected_crc)
    {
        return GROUND_STATION_PROTOCOL_CRC_ERROR;
    }

    frame->message_type = (GroundStationMessageType)input[3];
    frame->flags = input[4];
    frame->sequence = GroundStationProtocol_ReadU16(&input[6]);
    frame->payload_length = payload_length;
    if (payload_length != 0u)
    {
        memcpy(
            frame->payload,
            &input[GROUND_STATION_FRAME_HEADER_SIZE],
            payload_length);
    }
    return GROUND_STATION_PROTOCOL_OK;
}
