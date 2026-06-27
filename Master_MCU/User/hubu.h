#ifndef __HUBU_H
#define __HUBU_H

void CompFilter_Simple(void);
void Get_Angle(float *roll, float *pitch, float *yaw);
void Get_Gyro(float *rollRate, float *pitchRate, float *yawRate);
void Yaw_Calibrate(void);
#endif
