#ifndef SLAVE_MCU_BSP_SLAVE_BOARD_H
#define SLAVE_MCU_BSP_SLAVE_BOARD_H
#include "stm32f10x.h"
#define SLAVE_SENSOR_PERIOD_MS 50u
#define SLAVE_DISPLAY_PERIOD_MS 200u
#define SLAVE_CALIBRATION_MS 10000u
#define SLAVE_KEY_DEBOUNCE_MS 20u
#define SLAVE_SERVO_HIGH_US 2000u
#define SLAVE_SERVO_LOW_US 1000u
#define SLAVE_LOW_BATTERY_V 10.50f
void slave_board_init_interrupts(void);
void slave_board_init(void);
void slave_board_start_tick(void);
void slave_board_feed_watchdog(void);
void slave_board_set_servo(uint16_t pulse_us);
void slave_board_set_low_battery(uint8_t low);
void slave_board_request_battery(uint32_t now_ms);
void slave_board_process_battery(uint32_t now_ms);
float slave_board_battery_voltage(void);
uint8_t slave_board_battery_valid(void);
uint32_t slave_board_adc_errors(void);
#endif
