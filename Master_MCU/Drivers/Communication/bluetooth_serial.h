#ifndef MASTER_MCU_DRIVERS_COMMUNICATION_BLUETOOTH_SERIAL_H
#define MASTER_MCU_DRIVERS_COMMUNICATION_BLUETOOTH_SERIAL_H

#include <stdio.h>
#include <stdint.h>

extern char bluetooth_rx_packet[];
extern volatile uint8_t bluetooth_rx_ready;

/* PID_Param_Parse返回的参数更新位，仅对本次有效帧置位。 */
#define PID_PARAM_UPDATE_PKP            (1UL << 0)
#define PID_PARAM_UPDATE_PKI            (1UL << 1)
#define PID_PARAM_UPDATE_PKD            (1UL << 2)
#define PID_PARAM_UPDATE_MID            (1UL << 3)
#define PID_PARAM_UPDATE_RKP            (1UL << 4)
#define PID_PARAM_UPDATE_RKI            (1UL << 5)
#define PID_PARAM_UPDATE_RKD            (1UL << 6)
#define PID_PARAM_UPDATE_YKP            (1UL << 7)
#define PID_PARAM_UPDATE_YKI            (1UL << 8)
#define PID_PARAM_UPDATE_YKD            (1UL << 9)
#define PID_PARAM_UPDATE_PAKP           (1UL << 10)
#define PID_PARAM_UPDATE_RAKP           (1UL << 11)
#define PID_PARAM_UPDATE_YAKP           (1UL << 12)
#define PID_PARAM_UPDATE_PAIM           (1UL << 13)
#define PID_PARAM_UPDATE_RAIM           (1UL << 14)
#define PID_PARAM_UPDATE_CONTROL_SPEED  (1UL << 15)

void bluetooth_serial_init(void);
/* Main-loop service; returns 1 when one command is ready to parse. */
uint8_t bluetooth_serial_process(void);
uint32_t bluetooth_serial_get_rx_overruns(void);
uint32_t bluetooth_serial_get_rx_frame_errors(void);
void bluetooth_serial_send_byte(uint8_t byte);
void bluetooth_serial_send_array(uint8_t *array, uint16_t length);
void bluetooth_serial_send_string(char *string);
void bluetooth_serial_send_number(uint32_t number, uint8_t length);
void bluetooth_serial_printf(char *format, ...);
void bluetooth_serial_send_dma(uint8_t *buffer, uint16_t length);
uint32_t bluetooth_serial_parse_parameters(void);
float bluetooth_serial_get_pitch_kp(void);
float bluetooth_serial_get_pitch_ki(void);
float bluetooth_serial_get_pitch_kd(void);
float bluetooth_serial_get_midpoint_offset(void);
float bluetooth_serial_get_roll_kp(void);
float bluetooth_serial_get_roll_ki(void);
float bluetooth_serial_get_roll_kd(void);
float bluetooth_serial_get_yaw_kp(void);
float bluetooth_serial_get_yaw_ki(void);
float bluetooth_serial_get_yaw_kd(void);
float bluetooth_serial_get_pitch_angle_kp(void);
float bluetooth_serial_get_roll_angle_kp(void);
float bluetooth_serial_get_yaw_angle_kp(void);
float bluetooth_serial_get_roll_target(void);
float bluetooth_serial_get_pitch_target(void);
uint8_t bluetooth_serial_get_base_duty(void);

#endif
