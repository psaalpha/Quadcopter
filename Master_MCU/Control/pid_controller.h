#ifndef MASTER_MCU_CONTROL_PID_CONTROLLER_H
#define MASTER_MCU_CONTROL_PID_CONTROLLER_H
#include <stdint.h>
void pid_set_base_duty(float value);
//void Drone_RollPitchYaw_PID_Control(float current_roll, float current_pitch, float current_yaw,float roll_rate,float pitch_rate,float yaw_rate);
void pid_update_angle_loop(float current_roll, float current_pitch, float current_yaw);
void pid_update_rate_loop(float roll_rate, float pitch_rate, float yaw_rate);
/* 从控每送来一帧有效数据调用一次；dt_s 是本地实测帧间隔（秒）。 */
void pid_update_navigation(float current_altitude_cm,
                                         int32_t flow_x, int32_t flow_y,
                                         float dt_s);
/* 退出辅助模式或传感器失效时单独清导航环，不扰动姿态内外环。 */
void pid_reset_navigation(void);
void pid_stop_motors(void);
uint16_t pid_get_motor_compare_front_left(void);
uint16_t pid_get_motor_compare_front_right(void);
uint16_t pid_get_motor_compare_back_left(void);
uint16_t pid_get_motor_compare_back_right(void);
void pid_reset(void);
void pid_set_pitch_kp(float p);
void pid_set_pitch_ki(float p);
void pid_set_pitch_kd(float d);
void pid_set_roll_kp(float p);
void pid_set_roll_ki(float p);
void pid_set_roll_kd(float d);
void pid_set_yaw_kp(float p);

void pid_set_yaw_ki(float p);

void pid_set_yaw_kd(float d);

float pid_get_yaw_error(void);
float pid_get_pitch_error(void);

float pid_get_yaw_output_snapshot(void);

void pid_set_pitch_target(float d);

void pid_set_roll_target(float d);
void pid_set_yaw_target(float d);
void pid_set_altitude_target(float d);
void pid_set_position_target(int32_t x, int32_t y);
void pid_set_altitude_kp(float p);
void pid_set_altitude_ki(float p);
void pid_set_altitude_kd(float d);
void pid_set_position_x_kp(float p);
void pid_set_position_x_ki(float p);
void pid_set_position_x_kd(float d);
void pid_set_position_y_kp(float p);
void pid_set_position_y_ki(float p);
void pid_set_position_y_kd(float d);
float pid_get_altitude_output(void);
float pid_get_position_roll_target(void);
float pid_get_position_pitch_target(void);
void pid_set_pitch_angle_kp(float p);
void pid_set_roll_angle_kp(float p);
void pid_set_yaw_angle_kp(float p);
#endif
