#ifndef __KALMAN_H
#define __KALMAN_H

#include "stm32f10x.h"

// 初始化
void Kalman_Roll_Init(void);
void Kalman_Pitch_Init(void);

// 核心滤波函数
float Kalman_Get_Roll(float gyro, float acc_angle, float dt);
float Kalman_Get_Pitch(float gyro, float acc_angle, float dt);

#endif
