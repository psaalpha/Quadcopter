#ifndef MASTER_FLIGHT_CONTROLLER_H
#define MASTER_FLIGHT_CONTROLLER_H
#include <stdint.h>
#include "flight_parameters.h"
void flight_controller_init(void);
void flight_controller_set_reset_flags(uint32_t flags);
void flight_controller_step(void);
void flight_controller_command(const flight_command_t *command,char *response,unsigned capacity);
void flight_controller_telemetry(char *response,unsigned capacity,unsigned page);
uint8_t flight_controller_maintenance(void);
void flight_controller_finish_maintenance(void);
void flight_controller_emergency_stop(void);
uint32_t flight_controller_last_imu_step(void);
#endif
