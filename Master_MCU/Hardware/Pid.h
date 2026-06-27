#ifndef __PID_H
#define __PID_H
void Set_Base_Duty(float value);
//void Drone_RollPitchYaw_PID_Control(float current_roll, float current_pitch, float current_yaw,float rollRate,float pitchRate,float yawRate);
void Drone_Outer_Angle_PID_Control(float current_roll, float current_pitch, float current_yaw);
void Drone_Inner_Rate_PID_Control(float rollRate, float pitchRate, float yawRate);
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
void Pitch_Angle_Kp_Get(float p);
void Roll_Angle_Kp_Get(float p); 
void Yaw_Angle_Kp_Get(float p);
#endif
