#ifndef SHARED_PROTOCOL_FLIGHT_PARAMETERS_H
#define SHARED_PROTOCOL_FLIGHT_PARAMETERS_H
#include <stdint.h>
typedef enum {
    FP_PKP,FP_PKI,FP_PKD,FP_MID,FP_RKP,FP_RKI,FP_RKD,FP_YKP,FP_YKI,FP_YKD,
    FP_PAKP,FP_RAKP,FP_YAKP,FP_PAIM,FP_RAIM,FP_THROTTLE,
    FP_AKP,FP_AKI,FP_AKD,FP_XKP,FP_XKI,FP_XKD,FP_YXKP,FP_YXKI,FP_YXKD,
    FP_FLOW_X_SCALE,FP_FLOW_Y_SCALE,FP_FLOW_YAW,FP_HOVER,FP_BAT_WARN,FP_BAT_CRIT,
    FP_ROLL_OFFSET,FP_PITCH_OFFSET,FP_DECLINATION,FP_NAV_ENABLE,
    FP_GYRO_X,FP_GYRO_Y,FP_GYRO_Z,FP_COUNT
} flight_parameter_id_t;
typedef struct { float value[FP_COUNT]; uint32_t revision; uint8_t dirty; } flight_parameters_t;
typedef struct { const char *name; float initial,minimum,maximum; uint8_t persistent; } flight_parameter_info_t;
typedef enum { FC_SET,FC_GET,FC_SAVE,FC_LOAD,FC_DEFAULTS,FC_GYRO_CAL,FC_YAW_ZERO,FC_BENCH,FC_STATUS } flight_command_op_t;
typedef struct { flight_command_op_t op; uint8_t id; float value; } flight_command_t;
const flight_parameter_info_t *flight_parameter_info(unsigned id);
void flight_parameters_defaults(flight_parameters_t *p);
int flight_parameter_set(flight_parameters_t *p,unsigned id,float value);
int flight_command_parse(const char *text,flight_command_t *command);
/* Float bit patterns are serialized explicitly in little endian, with schema. */
#define FLIGHT_PARAMETERS_BYTES (4u + 4u * FP_COUNT)
void flight_parameters_encode(const flight_parameters_t *p,uint8_t *bytes);
int flight_parameters_decode(flight_parameters_t *p,const uint8_t *bytes,unsigned size);
#endif
