#ifndef MASTER_MCU_BSP_MOTOR_PWM_H
#define MASTER_MCU_BSP_MOTOR_PWM_H

#include <stdint.h>


void motor_pwm_init(void);
void motor_pwm_set_minimum_output(void);
void motor_pwm_set_compare1(uint16_t compare);
void motor_pwm_set_compare2(uint16_t compare);
void motor_pwm_set_compare3(uint16_t compare);
void motor_pwm_set_compare4(uint16_t compare);
#endif
