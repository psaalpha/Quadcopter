#ifndef __PID_H
#define __PID_H
#include "stm32f10x.h"
void Set_Base_Duty(float value);
//void Drone_RollPitchYaw_PID_Control(float current_roll, float current_pitch, float current_yaw,float rollRate,float pitchRate,float yawRate);
void Drone_Outer_Angle_PID_Control(float current_roll, float current_pitch, float current_yaw);
void Drone_Inner_Rate_PID_Control(float rollRate, float pitchRate, float yawRate);
void Drone_Altitude_Position_PID_Control(float current_altitude_cm, int32_t flow_x, int32_t flow_y);
void Drone_Motors_Stop(void);
uint16_t Get_Motor_Duty_FrontLeft(void); 
uint16_t Get_Motor_Duty_FrontRight(void); 
uint16_t Get_Motor_Duty_BackLeft(void);  
uint16_t Get_Motor_Duty_BackRight(void); 
void Drone_PID_Reset(void);
void Pitch_Kp_Get(float p);
void Pitch_Ki_Get(float p);
void Pitch_Kd_Get(float d);
void Roll_Kp_Get(float p);
void Roll_Ki_Get(float p);
void Roll_Kd_Get(float d);
void Yaw_Kp_Get(float p);

void Yaw_Ki_Get(float p);

void Yaw_Kd_Get(float d);

float Yaw_err_Get(void);
float Pitch_err_Get(void);

float Yaw_pid_Get(void);

void Pitch_aim_Get(float d);

void Roll_aim_Get(float d);
void Yaw_aim_Get(float d);
void Altitude_aim_Get(float d);
void Position_aim_Get(int32_t x, int32_t y);
void Altitude_Kp_Get(float p);
void Altitude_Ki_Get(float p);
void Altitude_Kd_Get(float d);
void Position_X_Kp_Get(float p);
void Position_X_Ki_Get(float p);
void Position_X_Kd_Get(float d);
void Position_Y_Kp_Get(float p);
void Position_Y_Ki_Get(float p);
void Position_Y_Kd_Get(float d);
float Altitude_pid_Get(void);
float Position_roll_aim_Get(void);
float Position_pitch_aim_Get(void);
void Pitch_Angle_Kp_Get(float p);
void Roll_Angle_Kp_Get(float p); 
void Yaw_Angle_Kp_Get(float p);
#endif
