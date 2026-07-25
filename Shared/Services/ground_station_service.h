#ifndef GROUND_STATION_SERVICE_H
#define GROUND_STATION_SERVICE_H

#include <stdint.h>

#include "ground_station_protocol.h"

#define GROUND_STATION_PARAMETER_NAME_MAX  31u

typedef enum
{
    GROUND_STATION_SERVICE_OK = 0,
    GROUND_STATION_SERVICE_INVALID_ARGUMENT,
    GROUND_STATION_SERVICE_UNSUPPORTED_MESSAGE,
    GROUND_STATION_SERVICE_BAD_PAYLOAD,
    GROUND_STATION_SERVICE_NOT_FOUND,
    GROUND_STATION_SERVICE_REJECTED,
    GROUND_STATION_SERVICE_INTERNAL_ERROR
} GroundStationServiceStatus;

typedef enum
{
    GROUND_STATION_COMMAND_DISARM = 1,
    GROUND_STATION_COMMAND_REQUEST_ARM,
    GROUND_STATION_COMMAND_CLEAR_FAULTS,
    GROUND_STATION_COMMAND_SAVE_PARAMETERS,
    GROUND_STATION_COMMAND_RESET
} GroundStationControlCommand;

typedef struct
{
    uint32_t uptime_ms;
    uint32_t fault_mask;
    uint16_t battery_mv;
    int16_t attitude_centi_deg[3];
    uint8_t system_state;
    uint8_t armed;
} GroundStationSystemStatus;

typedef struct
{
    uint16_t id;
    uint8_t type;
    uint8_t category;
    uint16_t flags;
    const char *name;
} GroundStationParameterInfo;

typedef GroundStationServiceStatus (*GroundStationGetStatusFn)(
    void *context,
    GroundStationSystemStatus *status);
typedef GroundStationServiceStatus (*GroundStationGetParameterFn)(
    void *context,
    uint16_t id,
    uint8_t *type,
    uint32_t *raw_value);
typedef GroundStationServiceStatus (*GroundStationSetParameterFn)(
    void *context,
    uint16_t id,
    uint8_t type,
    uint32_t raw_value);
typedef GroundStationServiceStatus (*GroundStationListParameterFn)(
    void *context,
    uint16_t index,
    GroundStationParameterInfo *info);
typedef GroundStationServiceStatus (*GroundStationControlFn)(
    void *context,
    GroundStationControlCommand command,
    int32_t argument);

typedef struct
{
    GroundStationGetStatusFn get_status;
    GroundStationGetParameterFn get_parameter;
    GroundStationSetParameterFn set_parameter;
    GroundStationListParameterFn list_parameter;
    GroundStationControlFn control;
    void *context;
} GroundStationServiceOps;

GroundStationServiceStatus GroundStationService_Process(
    const GroundStationServiceOps *service,
    const GroundStationFrame *request,
    GroundStationFrame *response);

#endif
