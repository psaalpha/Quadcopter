#include "ground_station_service.h"

#include <string.h>

static uint16_t GroundStationService_ReadU16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0]
        | ((uint16_t)data[1] << 8));
}

static uint32_t GroundStationService_ReadU32(const uint8_t *data)
{
    return (uint32_t)data[0]
        | ((uint32_t)data[1] << 8)
        | ((uint32_t)data[2] << 16)
        | ((uint32_t)data[3] << 24);
}

static void GroundStationService_WriteU16(
    uint8_t *data,
    uint16_t value)
{
    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void GroundStationService_WriteU32(
    uint8_t *data,
    uint32_t value)
{
    data[0] = (uint8_t)(value & 0xFFu);
    data[1] = (uint8_t)((value >> 8) & 0xFFu);
    data[2] = (uint8_t)((value >> 16) & 0xFFu);
    data[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static GroundStationServiceStatus GroundStationService_Error(
    const GroundStationFrame *request,
    GroundStationFrame *response,
    GroundStationServiceStatus error)
{
    response->message_type = GROUND_STATION_MESSAGE_ERROR_RESPONSE;
    response->flags = 0u;
    response->sequence = request->sequence;
    response->payload_length = 2u;
    response->payload[0] = (uint8_t)request->message_type;
    response->payload[1] = (uint8_t)error;
    return error;
}

static GroundStationServiceStatus GroundStationService_GetStatus(
    const GroundStationServiceOps *service,
    const GroundStationFrame *request,
    GroundStationFrame *response)
{
    GroundStationSystemStatus status;
    GroundStationServiceStatus result;

    if ((request->payload_length != 0u) || (service->get_status == 0))
    {
        return GroundStationService_Error(
            request,
            response,
            GROUND_STATION_SERVICE_BAD_PAYLOAD);
    }

    result = service->get_status(service->context, &status);
    if (result != GROUND_STATION_SERVICE_OK)
    {
        return GroundStationService_Error(request, response, result);
    }

    response->message_type = GROUND_STATION_MESSAGE_GET_STATUS_RESPONSE;
    response->payload_length = 18u;
    GroundStationService_WriteU32(&response->payload[0], status.uptime_ms);
    GroundStationService_WriteU32(&response->payload[4], status.fault_mask);
    GroundStationService_WriteU16(&response->payload[8], status.battery_mv);
    GroundStationService_WriteU16(
        &response->payload[10],
        (uint16_t)status.attitude_centi_deg[0]);
    GroundStationService_WriteU16(
        &response->payload[12],
        (uint16_t)status.attitude_centi_deg[1]);
    GroundStationService_WriteU16(
        &response->payload[14],
        (uint16_t)status.attitude_centi_deg[2]);
    response->payload[16] = status.system_state;
    response->payload[17] = status.armed;
    return GROUND_STATION_SERVICE_OK;
}

static GroundStationServiceStatus GroundStationService_GetParameter(
    const GroundStationServiceOps *service,
    const GroundStationFrame *request,
    GroundStationFrame *response)
{
    GroundStationServiceStatus result;
    uint16_t id;
    uint8_t type = 0u;
    uint32_t raw_value = 0u;

    if ((request->payload_length != 2u)
        || (service->get_parameter == 0))
    {
        return GroundStationService_Error(
            request,
            response,
            GROUND_STATION_SERVICE_BAD_PAYLOAD);
    }

    id = GroundStationService_ReadU16(request->payload);
    result = service->get_parameter(
        service->context,
        id,
        &type,
        &raw_value);
    if (result != GROUND_STATION_SERVICE_OK)
    {
        return GroundStationService_Error(request, response, result);
    }

    response->message_type = GROUND_STATION_MESSAGE_PARAMETER_GET_RESPONSE;
    response->payload_length = 8u;
    GroundStationService_WriteU16(&response->payload[0], id);
    response->payload[2] = type;
    response->payload[3] = 0u;
    GroundStationService_WriteU32(&response->payload[4], raw_value);
    return GROUND_STATION_SERVICE_OK;
}

static GroundStationServiceStatus GroundStationService_SetParameter(
    const GroundStationServiceOps *service,
    const GroundStationFrame *request,
    GroundStationFrame *response)
{
    GroundStationServiceStatus result;
    uint16_t id;
    uint8_t type;
    uint32_t raw_value;

    if ((request->payload_length != 8u)
        || (service->set_parameter == 0))
    {
        return GroundStationService_Error(
            request,
            response,
            GROUND_STATION_SERVICE_BAD_PAYLOAD);
    }

    id = GroundStationService_ReadU16(&request->payload[0]);
    type = request->payload[2];
    raw_value = GroundStationService_ReadU32(&request->payload[4]);
    result = service->set_parameter(
        service->context,
        id,
        type,
        raw_value);
    if (result != GROUND_STATION_SERVICE_OK)
    {
        return GroundStationService_Error(request, response, result);
    }

    response->message_type = GROUND_STATION_MESSAGE_PARAMETER_SET_RESPONSE;
    response->payload_length = 4u;
    GroundStationService_WriteU16(&response->payload[0], id);
    response->payload[2] = type;
    response->payload[3] = 0u;
    return GROUND_STATION_SERVICE_OK;
}

static GroundStationServiceStatus GroundStationService_ListParameter(
    const GroundStationServiceOps *service,
    const GroundStationFrame *request,
    GroundStationFrame *response)
{
    GroundStationParameterInfo info;
    GroundStationServiceStatus result;
    uint16_t index;
    uint16_t name_length;

    if ((request->payload_length != 2u)
        || (service->list_parameter == 0))
    {
        return GroundStationService_Error(
            request,
            response,
            GROUND_STATION_SERVICE_BAD_PAYLOAD);
    }

    index = GroundStationService_ReadU16(request->payload);
    result = service->list_parameter(service->context, index, &info);
    if (result != GROUND_STATION_SERVICE_OK)
    {
        return GroundStationService_Error(request, response, result);
    }
    if (info.name == 0)
    {
        return GroundStationService_Error(
            request,
            response,
            GROUND_STATION_SERVICE_INTERNAL_ERROR);
    }

    name_length = (uint16_t)strlen(info.name);
    if (name_length > GROUND_STATION_PARAMETER_NAME_MAX)
    {
        name_length = GROUND_STATION_PARAMETER_NAME_MAX;
    }

    response->message_type = GROUND_STATION_MESSAGE_PARAMETER_LIST_RESPONSE;
    response->payload_length = (uint16_t)(9u + name_length);
    GroundStationService_WriteU16(&response->payload[0], index);
    GroundStationService_WriteU16(&response->payload[2], info.id);
    response->payload[4] = info.type;
    response->payload[5] = info.category;
    GroundStationService_WriteU16(&response->payload[6], info.flags);
    response->payload[8] = (uint8_t)name_length;
    if (name_length != 0u)
    {
        memcpy(&response->payload[9], info.name, name_length);
    }
    return GROUND_STATION_SERVICE_OK;
}

static GroundStationServiceStatus GroundStationService_Control(
    const GroundStationServiceOps *service,
    const GroundStationFrame *request,
    GroundStationFrame *response)
{
    GroundStationControlCommand command;
    GroundStationServiceStatus result;
    int32_t argument;

    if ((request->payload_length != 8u) || (service->control == 0))
    {
        return GroundStationService_Error(
            request,
            response,
            GROUND_STATION_SERVICE_BAD_PAYLOAD);
    }

    command = (GroundStationControlCommand)request->payload[0];
    if ((command < GROUND_STATION_COMMAND_DISARM)
        || (command > GROUND_STATION_COMMAND_RESET))
    {
        return GroundStationService_Error(
            request,
            response,
            GROUND_STATION_SERVICE_BAD_PAYLOAD);
    }
    argument = (int32_t)GroundStationService_ReadU32(&request->payload[4]);
    result = service->control(service->context, command, argument);
    if (result != GROUND_STATION_SERVICE_OK)
    {
        return GroundStationService_Error(request, response, result);
    }

    response->message_type = GROUND_STATION_MESSAGE_CONTROL_RESPONSE;
    response->payload_length = 4u;
    response->payload[0] = (uint8_t)command;
    response->payload[1] = 0u;
    response->payload[2] = 0u;
    response->payload[3] = 0u;
    return GROUND_STATION_SERVICE_OK;
}

GroundStationServiceStatus GroundStationService_Process(
    const GroundStationServiceOps *service,
    const GroundStationFrame *request,
    GroundStationFrame *response)
{
    if ((service == 0) || (request == 0) || (response == 0))
    {
        return GROUND_STATION_SERVICE_INVALID_ARGUMENT;
    }

    response->flags = 0u;
    response->sequence = request->sequence;
    response->payload_length = 0u;

    switch (request->message_type)
    {
        case GROUND_STATION_MESSAGE_GET_STATUS_REQUEST:
            return GroundStationService_GetStatus(
                service,
                request,
                response);
        case GROUND_STATION_MESSAGE_PARAMETER_GET_REQUEST:
            return GroundStationService_GetParameter(
                service,
                request,
                response);
        case GROUND_STATION_MESSAGE_PARAMETER_SET_REQUEST:
            return GroundStationService_SetParameter(
                service,
                request,
                response);
        case GROUND_STATION_MESSAGE_PARAMETER_LIST_REQUEST:
            return GroundStationService_ListParameter(
                service,
                request,
                response);
        case GROUND_STATION_MESSAGE_CONTROL_REQUEST:
            return GroundStationService_Control(
                service,
                request,
                response);
        default:
            return GroundStationService_Error(
                request,
                response,
                GROUND_STATION_SERVICE_UNSUPPORTED_MESSAGE);
    }
}
