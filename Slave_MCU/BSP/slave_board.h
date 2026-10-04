#ifndef SLAVE_BOARD_H
#define SLAVE_BOARD_H
#include "stm32f10x.h"
#define SLAVE_SENSOR_PERIOD_MS 50u
#define SLAVE_DISPLAY_PERIOD_MS 200u
#define SLAVE_CALIBRATION_MS 10000u
#define SLAVE_KEY_DEBOUNCE_MS 20u
#define SLAVE_SERVO_HIGH_US 2000u
#define SLAVE_SERVO_LOW_US 1000u
#define SLAVE_LOW_BATTERY_V 10.50f
void SlaveBoard_InitInterrupts(void);
void SlaveBoard_Init(void);
void SlaveBoard_StartTick(void);
void SlaveBoard_FeedWatchdog(void);
void SlaveBoard_SetServo(uint16_t pulse_us);
void SlaveBoard_SetLowBattery(uint8_t low);
void SlaveBoard_RequestBattery(uint32_t now_ms);
void SlaveBoard_ProcessBattery(uint32_t now_ms);
float SlaveBoard_BatteryVoltage(void);
uint8_t SlaveBoard_BatteryValid(void);
uint32_t SlaveBoard_AdcErrors(void);
#endif
