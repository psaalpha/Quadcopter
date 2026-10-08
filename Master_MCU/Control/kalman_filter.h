#ifndef MASTER_MCU_CONTROL_KALMAN_FILTER_H
#define MASTER_MCU_CONTROL_KALMAN_FILTER_H

#include "stm32f10x.h"

// 初始化
void kalman_init_roll(void);
void kalman_init_pitch(void);

// 核心滤波函数
float kalman_update_roll(float gyro, float acc_angle, float dt);
float kalman_update_pitch(float gyro, float acc_angle, float dt);

#endif
