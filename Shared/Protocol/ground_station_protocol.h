#ifndef GROUND_STATION_PROTOCOL_H
#define GROUND_STATION_PROTOCOL_H

#include <stdint.h>

#define GROUND_STATION_PROTOCOL_VERSION      1u
#define GROUND_STATION_MAX_PAYLOAD_SIZE      48u
#define GROUND_STATION_FRAME_HEADER_SIZE     10u
#define GROUND_STATION_FRAME_CRC_SIZE        2u
#define GROUND_STATION_MAX_FRAME_SIZE        \
    (GROUND_STATION_FRAME_HEADER_SIZE         \
        + GROUND_STATION_MAX_PAYLOAD_SIZE     \
        + GROUND_STATION_FRAME_CRC_SIZE)

typedef enum
{
    GROUND_STATION_MESSAGE_GET_STATUS_REQUEST = 0x01,
    GROUND_STATION_MESSAGE_PARAMETER_GET_REQUEST = 0x02,
    GROUND_STATION_MESSAGE_PARAMETER_SET_REQUEST = 0x03,
    GROUND_STATION_MESSAGE_PARAMETER_LIST_REQUEST = 0x04,
    GROUND_STATION_MESSAGE_CONTROL_REQUEST = 0x05,

    GROUND_STATION_MESSAGE_GET_STATUS_RESPONSE = 0x81,
    GROUND_STATION_MESSAGE_PARAMETER_GET_RESPONSE = 0x82,
    GROUND_STATION_MESSAGE_PARAMETER_SET_RESPONSE = 0x83,
    GROUND_STATION_MESSAGE_PARAMETER_LIST_RESPONSE = 0x84,
    GROUND_STATION_MESSAGE_CONTROL_RESPONSE = 0x85,
    GROUND_STATION_MESSAGE_ERROR_RESPONSE = 0xFF
} GroundStationMessageType;

typedef enum
{
    GROUND_STATION_PROTOCOL_OK = 0,
    GROUND_STATION_PROTOCOL_INVALID_ARGUMENT,
    GROUND_STATION_PROTOCOL_BUFFER_TOO_SMALL,
    GROUND_STATION_PROTOCOL_BAD_MAGIC,
    GROUND_STATION_PROTOCOL_UNSUPPORTED_VERSION,
    GROUND_STATION_PROTOCOL_BAD_LENGTH,
    GROUND_STATION_PROTOCOL_CRC_ERROR
} GroundStationProtocolStatus;

typedef struct
{
    GroundStationMessageType message_type;
    uint8_t flags;
    uint16_t sequence;
    uint16_t payload_length;
    uint8_t payload[GROUND_STATION_MAX_PAYLOAD_SIZE];
} GroundStationFrame;

uint16_t GroundStationProtocol_Crc16(
    const uint8_t *data,
    uint16_t length);
GroundStationProtocolStatus GroundStationProtocol_Encode(
    const GroundStationFrame *frame,
    uint8_t *output,
    uint16_t capacity,
    uint16_t *written_size);
GroundStationProtocolStatus GroundStationProtocol_Decode(
    GroundStationFrame *frame,
    const uint8_t *input,
    uint16_t input_size);

#endif
