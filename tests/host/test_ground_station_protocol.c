#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "ground_station_protocol.h"
#include "ground_station_service.h"

typedef struct
{
    uint16_t parameter_id;
    uint8_t parameter_type;
    uint32_t parameter_value;
    GroundStationControlCommand last_command;
    int32_t last_argument;
    uint8_t control_calls;
} FakeGroundStationContext;

static uint16_t ReadU16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0]
        | ((uint16_t)data[1] << 8));
}

static uint32_t ReadU32(const uint8_t *data)
{
    return (uint32_t)data[0]
        | ((uint32_t)data[1] << 8)
        | ((uint32_t)data[2] << 16)
        | ((uint32_t)data[3] << 24);
}

static void WriteU16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)(value >> 8);
}

static void WriteU32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static GroundStationServiceStatus FakeGetStatus(
    void *context,
    GroundStationSystemStatus *status)
{
    (void)context;
    status->uptime_ms = 123456u;
    status->fault_mask = 0x00000012u;
    status->battery_mv = 11800u;
    status->attitude_centi_deg[0] = -120;
    status->attitude_centi_deg[1] = 230;
    status->attitude_centi_deg[2] = 4500;
    status->system_state = 2u;
    status->armed = 1u;
    return GROUND_STATION_SERVICE_OK;
}

static GroundStationServiceStatus FakeGetParameter(
    void *context,
    uint16_t id,
    uint8_t *type,
    uint32_t *raw_value)
{
    FakeGroundStationContext *fake =
        (FakeGroundStationContext *)context;

    if (id != fake->parameter_id)
    {
        return GROUND_STATION_SERVICE_NOT_FOUND;
    }
    *type = fake->parameter_type;
    *raw_value = fake->parameter_value;
    return GROUND_STATION_SERVICE_OK;
}

static GroundStationServiceStatus FakeSetParameter(
    void *context,
    uint16_t id,
    uint8_t type,
    uint32_t raw_value)
{
    FakeGroundStationContext *fake =
        (FakeGroundStationContext *)context;

    if (id != fake->parameter_id)
    {
        return GROUND_STATION_SERVICE_NOT_FOUND;
    }
    fake->parameter_type = type;
    fake->parameter_value = raw_value;
    return GROUND_STATION_SERVICE_OK;
}

static GroundStationServiceStatus FakeListParameter(
    void *context,
    uint16_t index,
    GroundStationParameterInfo *info)
{
    FakeGroundStationContext *fake =
        (FakeGroundStationContext *)context;

    if (index != 0u)
    {
        return GROUND_STATION_SERVICE_NOT_FOUND;
    }
    info->id = fake->parameter_id;
    info->type = fake->parameter_type;
    info->category = 1u;
    info->flags = 0x0003u;
    info->name = "control.roll.kp";
    return GROUND_STATION_SERVICE_OK;
}

static GroundStationServiceStatus FakeControl(
    void *context,
    GroundStationControlCommand command,
    int32_t argument)
{
    FakeGroundStationContext *fake =
        (FakeGroundStationContext *)context;

    fake->last_command = command;
    fake->last_argument = argument;
    ++fake->control_calls;
    return (command == GROUND_STATION_COMMAND_REQUEST_ARM)
        ? GROUND_STATION_SERVICE_REJECTED
        : GROUND_STATION_SERVICE_OK;
}

static GroundStationServiceOps MakeService(
    FakeGroundStationContext *context)
{
    GroundStationServiceOps service;

    service.get_status = FakeGetStatus;
    service.get_parameter = FakeGetParameter;
    service.set_parameter = FakeSetParameter;
    service.list_parameter = FakeListParameter;
    service.control = FakeControl;
    service.context = context;
    return service;
}

static void TestFrameCodec(void)
{
    GroundStationFrame source;
    GroundStationFrame decoded;
    uint8_t encoded[GROUND_STATION_MAX_FRAME_SIZE];
    uint16_t encoded_size = 0u;

    memset(&source, 0, sizeof(source));
    source.message_type = GROUND_STATION_MESSAGE_PARAMETER_GET_REQUEST;
    source.flags = 0x03u;
    source.sequence = 0x1234u;
    source.payload_length = 2u;
    WriteU16(source.payload, 7u);

    assert(GroundStationProtocol_Encode(
        &source,
        encoded,
        sizeof(encoded),
        &encoded_size) == GROUND_STATION_PROTOCOL_OK);
    assert(encoded_size == 14u);
    assert(GroundStationProtocol_Decode(
        &decoded,
        encoded,
        encoded_size) == GROUND_STATION_PROTOCOL_OK);
    assert(decoded.message_type == source.message_type);
    assert(decoded.flags == source.flags);
    assert(decoded.sequence == source.sequence);
    assert(decoded.payload_length == source.payload_length);
    assert(memcmp(decoded.payload, source.payload, 2u) == 0);

    encoded[10] ^= 0x01u;
    assert(GroundStationProtocol_Decode(
        &decoded,
        encoded,
        encoded_size) == GROUND_STATION_PROTOCOL_CRC_ERROR);
    encoded[10] ^= 0x01u;
    encoded[0] = 0u;
    assert(GroundStationProtocol_Decode(
        &decoded,
        encoded,
        encoded_size) == GROUND_STATION_PROTOCOL_BAD_MAGIC);
}

static void TestStatusAndParameters(void)
{
    FakeGroundStationContext fake;
    GroundStationServiceOps service;
    GroundStationFrame request;
    GroundStationFrame response;

    memset(&fake, 0, sizeof(fake));
    fake.parameter_id = 7u;
    fake.parameter_type = 2u;
    fake.parameter_value = 0x3F800000u;
    service = MakeService(&fake);

    memset(&request, 0, sizeof(request));
    request.message_type = GROUND_STATION_MESSAGE_GET_STATUS_REQUEST;
    request.sequence = 42u;
    assert(GroundStationService_Process(
        &service,
        &request,
        &response) == GROUND_STATION_SERVICE_OK);
    assert(response.message_type
        == GROUND_STATION_MESSAGE_GET_STATUS_RESPONSE);
    assert(response.sequence == 42u);
    assert(response.payload_length == 18u);
    assert(ReadU32(&response.payload[0]) == 123456u);
    assert(ReadU32(&response.payload[4]) == 0x12u);
    assert(ReadU16(&response.payload[8]) == 11800u);
    assert((int16_t)ReadU16(&response.payload[10]) == -120);
    assert(response.payload[17] == 1u);

    request.message_type = GROUND_STATION_MESSAGE_PARAMETER_GET_REQUEST;
    request.payload_length = 2u;
    WriteU16(request.payload, 7u);
    assert(GroundStationService_Process(
        &service,
        &request,
        &response) == GROUND_STATION_SERVICE_OK);
    assert(response.message_type
        == GROUND_STATION_MESSAGE_PARAMETER_GET_RESPONSE);
    assert(ReadU16(&response.payload[0]) == 7u);
    assert(response.payload[2] == 2u);
    assert(ReadU32(&response.payload[4]) == 0x3F800000u);

    request.message_type = GROUND_STATION_MESSAGE_PARAMETER_SET_REQUEST;
    request.payload_length = 8u;
    WriteU16(&request.payload[0], 7u);
    request.payload[2] = 3u;
    request.payload[3] = 0u;
    WriteU32(&request.payload[4], 0x40000000u);
    assert(GroundStationService_Process(
        &service,
        &request,
        &response) == GROUND_STATION_SERVICE_OK);
    assert(fake.parameter_type == 3u);
    assert(fake.parameter_value == 0x40000000u);

    request.message_type = GROUND_STATION_MESSAGE_PARAMETER_LIST_REQUEST;
    request.payload_length = 2u;
    WriteU16(request.payload, 0u);
    assert(GroundStationService_Process(
        &service,
        &request,
        &response) == GROUND_STATION_SERVICE_OK);
    assert(response.message_type
        == GROUND_STATION_MESSAGE_PARAMETER_LIST_RESPONSE);
    assert(response.payload[8] == 15u);
    assert(memcmp(&response.payload[9], "control.roll.kp", 15u) == 0);
}

static void TestControlSafetyBoundary(void)
{
    FakeGroundStationContext fake;
    GroundStationServiceOps service;
    GroundStationFrame request;
    GroundStationFrame response;

    memset(&fake, 0, sizeof(fake));
    service = MakeService(&fake);
    memset(&request, 0, sizeof(request));
    request.message_type = GROUND_STATION_MESSAGE_CONTROL_REQUEST;
    request.payload_length = 8u;
    request.payload[0] = GROUND_STATION_COMMAND_DISARM;
    WriteU32(&request.payload[4], 99u);
    assert(GroundStationService_Process(
        &service,
        &request,
        &response) == GROUND_STATION_SERVICE_OK);
    assert(fake.control_calls == 1u);
    assert(fake.last_command == GROUND_STATION_COMMAND_DISARM);
    assert(fake.last_argument == 99);

    request.payload[0] = GROUND_STATION_COMMAND_REQUEST_ARM;
    assert(GroundStationService_Process(
        &service,
        &request,
        &response) == GROUND_STATION_SERVICE_REJECTED);
    assert(response.message_type == GROUND_STATION_MESSAGE_ERROR_RESPONSE);
    assert(response.payload[1] == GROUND_STATION_SERVICE_REJECTED);

    request.payload[0] = 0xFFu;
    assert(GroundStationService_Process(
        &service,
        &request,
        &response) == GROUND_STATION_SERVICE_BAD_PAYLOAD);
    assert(fake.control_calls == 2u);
}

int main(void)
{
    TestFrameCodec();
    TestStatusAndParameters();
    TestControlSafetyBoundary();
    return 0;
}
