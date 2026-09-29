#ifndef __HUBU_H
#define __HUBU_H

#include <stdint.h>

void CompFilter_Simple(void);
void Get_Angle(float *roll, float *pitch, float *yaw);
void Get_Gyro(float *rollRate, float *pitchRate, float *yawRate);
/* 从控磁力计有效新帧到来时调用；首次对齐返回 1，供上层同步目标航向。 */
uint8_t Yaw_ApplyMagHeading(float heading_deg);
void Yaw_Calibrate(void);
#endif
