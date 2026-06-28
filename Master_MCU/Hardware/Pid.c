#include "stm32f10x.h"
#include <math.h>
#include "hubu.h"

/* 四轴电机输出占空比，范围 0~100% */
static float Motor_Duty_FrontLeft  = 0.0f;
static float Motor_Duty_FrontRight = 0.0f;
static float Motor_Duty_BackLeft   = 0.0f;
static float Motor_Duty_BackRight  = 0.0f;

/* 控制器核心配置 */
static float BASE_DUTY = 0.0f;   
#define PWM_MAX            100.0f          
#define PWM_MIN             0.0f           
#define ANGLE_LIMIT        30.0f           
#define YAW_ANGLE_LIMIT    180.0f          
#define PID_SAMPLE_TIME    0.002f  /* 2ms = 500Hz 内环频率 */

/* 角速度低通滤波系数。
 * hubu.c 中已对 gx/gy 做过滤波，这里默认透传；如需二次滤波可设为 < 1.0f。
 */
#define GYRO_LPF_ALPHA 1.0f
static float last_rollRate = 0;
static float last_pitchRate = 0;
static float last_yawRate = 0;

/* 外环：角度环参数，当前仅使用 P 控制 */
float Roll_Outer_Kp  =12.03f;   /* 决定响应快慢：过大易震荡，过小响应迟钝 */
float Pitch_Outer_Kp = 6.03f;   
float Yaw_Outer_Kp   = 5.72f;   /* Yaw 外环 P 通常小于 Roll/Pitch */

/* 外环输出的目标角速度，供内环使用 */
static float target_roll_rate  = 0.0f;
static float target_pitch_rate = 0.0f;
static float target_yaw_rate   = 0.0f;

/* 内环：角速度环 PID 参数 */
/* Roll 内环 */
float Roll_Inner_Kp = 0.80f;    /* P：抵消目标角速度与实测角速度的误差 */
float Roll_Inner_Ki = 0.105f;   /* I：消除机械偏心等造成的静态误差 */
float Roll_Inner_Kd = 0.0018f;  /* D：抑制角速度震荡 */
static float Roll_Inner_I_Limit = 5.0f;

/* Pitch 内环 */
float Pitch_Inner_Kp = 0.179f;
float Pitch_Inner_Ki = 0.1058f;
float Pitch_Inner_Kd = 0.001838f;
static float Pitch_Inner_I_Limit = 5.0f;

/* Yaw 内环 */
float Yaw_Inner_Kp = 2.87f;
float Yaw_Inner_Ki = 0.0251f;
float Yaw_Inner_Kd = 0.0034444f;
static float Yaw_Inner_I_Limit = 3.0f;

/* D 项低通滤波系数：0.0=滤波最强但延迟最大，1.0=不滤波。 */
#define DTERM_LPF_ALPHA 0.3f

/* Altitude and optical-flow position PID run from slave sensor updates. */
#define NAV_PID_SAMPLE_TIME      0.02f
#define ALTITUDE_I_LIMIT        10.0f
#define ALTITUDE_OUT_LIMIT      20.0f
#define POSITION_I_LIMIT         5.0f
#define POSITION_ANGLE_LIMIT    10.0f

/* 内环积分项 */
static float roll_rate_integral  = 0.0f;
static float pitch_rate_integral = 0.0f;
static float yaw_rate_integral   = 0.0f;

/* D on measurement：使用测量值差分，避免目标突变造成微分冲击 */
static float last_roll_rate   = 0.0f;
static float last_pitch_rate  = 0.0f;
static float last_yaw_rate    = 0.0f;

/* D 项滤波历史值 */
static float roll_d_filtered  = 0.0f;
static float pitch_d_filtered = 0.0f;
static float yaw_d_filtered   = 0.0f;

/* PID 输出，供电机混控使用 */
float roll_pid_out;
float pitch_pid_out;
float yaw_pid_out;

/* 遥控或上层控制给出的目标角度 */
float target_roll  = 0.0f;
float target_pitch = 0.0f;
float target_yaw   = 0.0f;

/* 调试观测变量 */
float yaw_err;
float yaw_ceshi = 0;
float Pitch_err;

/* Altitude hold PID: output is throttle correction in duty percent. */
float Altitude_Kp = 0.0f;
float Altitude_Ki = 0.0f;
float Altitude_Kd = 0.0f;
static float target_altitude_cm = 0.0f;
static float altitude_integral = 0.0f;
static float last_altitude_err = 0.0f;
static float altitude_pid_out = 0.0f;

/* Optical-flow position PID: outputs are suggested angle corrections. */
float Position_X_Kp = 0.0f;
float Position_X_Ki = 0.0f;
float Position_X_Kd = 0.0f;
float Position_Y_Kp = 0.0f;
float Position_Y_Ki = 0.0f;
float Position_Y_Kd = 0.0f;
static int32_t target_flow_x = 0;
static int32_t target_flow_y = 0;
static float position_x_integral = 0.0f;
static float position_y_integral = 0.0f;
static float last_position_x_err = 0.0f;
static float last_position_y_err = 0.0f;
static float position_roll_aim = 0.0f;
static float position_pitch_aim = 0.0f;

static float Limit_Float(float value, float min, float max)
{
    if(value > max) return max;
    if(value < min) return min;
    return value;
}

/* 参数设置接口 */
void Set_Base_Duty(float value) {
    if(value >= 0.0f && value <= 100.0f) BASE_DUTY = value;
}

/* 外环参数设置 */
void Roll_Outer_Kp_Get(float p)  { Roll_Outer_Kp = p; }
void Pitch_Outer_Kp_Get(float p) { Pitch_Outer_Kp = p; }
void Yaw_Outer_Kp_Get(float p)   { Yaw_Outer_Kp = p; }

/* 内环参数设置 */
void Roll_Inner_Kp_Get(float p)  { Roll_Inner_Kp = p; }
void Roll_Inner_Ki_Get(float p)  { Roll_Inner_Ki = p; }
void Roll_Inner_Kd_Get(float d)  { Roll_Inner_Kd = d; }

void Pitch_Inner_Kp_Get(float p) { Pitch_Inner_Kp = p; }
void Pitch_Inner_Ki_Get(float p) { Pitch_Inner_Ki = p; }
void Pitch_Inner_Kd_Get(float d) { Pitch_Inner_Kd = d; }

void Yaw_Inner_Kp_Get(float p)   { Yaw_Inner_Kp = p; }
void Yaw_Inner_Ki_Get(float p)   { Yaw_Inner_Ki = p; }
void Yaw_Inner_Kd_Get(float d)   { Yaw_Inner_Kd = d; }

void Pitch_aim_Get(float d) { target_pitch = d; }
void Roll_aim_Get(float d)  { target_roll = d; }
void Yaw_aim_Get(float d)   { target_yaw = d; }
void Altitude_aim_Get(float d) { target_altitude_cm = d; }
void Position_aim_Get(int32_t x, int32_t y)
{
    target_flow_x = x;
    target_flow_y = y;
}

void Altitude_Kp_Get(float p) { Altitude_Kp = p; }
void Altitude_Ki_Get(float p) { Altitude_Ki = p; }
void Altitude_Kd_Get(float d) { Altitude_Kd = d; }
void Position_X_Kp_Get(float p) { Position_X_Kp = p; }
void Position_X_Ki_Get(float p) { Position_X_Ki = p; }
void Position_X_Kd_Get(float d) { Position_X_Kd = d; }
void Position_Y_Kp_Get(float p) { Position_Y_Kp = p; }
void Position_Y_Ki_Get(float p) { Position_Y_Ki = p; }
void Position_Y_Kd_Get(float d) { Position_Y_Kd = d; }

float Pitch_err_Get(void) { return Pitch_err; }
float Yaw_err_Get(void) { return yaw_err; }
float Yaw_pid_Get(void) { return yaw_ceshi; }
float Altitude_pid_Get(void) { return altitude_pid_out; }
float Position_roll_aim_Get(void) { return position_roll_aim; }
float Position_pitch_aim_Get(void) { return position_pitch_aim; }

void Drone_Altitude_Position_PID_Control(float current_altitude_cm, int32_t flow_x, int32_t flow_y)
{
    float altitude_err = target_altitude_cm - current_altitude_cm;
    altitude_integral += Altitude_Ki * altitude_err * NAV_PID_SAMPLE_TIME;
    altitude_integral = Limit_Float(altitude_integral, -ALTITUDE_I_LIMIT, ALTITUDE_I_LIMIT);

    float altitude_d = Altitude_Kd * (altitude_err - last_altitude_err) / NAV_PID_SAMPLE_TIME;
    last_altitude_err = altitude_err;
    altitude_pid_out = Altitude_Kp * altitude_err + altitude_integral + altitude_d;
    altitude_pid_out = Limit_Float(altitude_pid_out, -ALTITUDE_OUT_LIMIT, ALTITUDE_OUT_LIMIT);

    float position_x_err = (float)(target_flow_x - flow_x);
    position_x_integral += Position_X_Ki * position_x_err * NAV_PID_SAMPLE_TIME;
    position_x_integral = Limit_Float(position_x_integral, -POSITION_I_LIMIT, POSITION_I_LIMIT);
    float position_x_d = Position_X_Kd * (position_x_err - last_position_x_err) / NAV_PID_SAMPLE_TIME;
    last_position_x_err = position_x_err;
    position_roll_aim = Position_X_Kp * position_x_err + position_x_integral + position_x_d;
    position_roll_aim = Limit_Float(position_roll_aim, -POSITION_ANGLE_LIMIT, POSITION_ANGLE_LIMIT);

    float position_y_err = (float)(target_flow_y - flow_y);
    position_y_integral += Position_Y_Ki * position_y_err * NAV_PID_SAMPLE_TIME;
    position_y_integral = Limit_Float(position_y_integral, -POSITION_I_LIMIT, POSITION_I_LIMIT);
    float position_y_d = Position_Y_Kd * (position_y_err - last_position_y_err) / NAV_PID_SAMPLE_TIME;
    last_position_y_err = position_y_err;
    position_pitch_aim = Position_Y_Kp * position_y_err + position_y_integral + position_y_d;
    position_pitch_aim = Limit_Float(position_pitch_aim, -POSITION_ANGLE_LIMIT, POSITION_ANGLE_LIMIT);

    if(BASE_DUTY <= 1)
    {
        altitude_integral = 0.0f;
        position_x_integral = 0.0f;
        position_y_integral = 0.0f;
        altitude_pid_out = 0.0f;
        position_roll_aim = 0.0f;
        position_pitch_aim = 0.0f;
    }
}



void Drone_Outer_Angle_PID_Control(float current_roll, float current_pitch, float current_yaw)
{
    /* 角度测量限幅，避免异常姿态值直接放大到外环输出 */
    if(current_roll > ANGLE_LIMIT) current_roll = ANGLE_LIMIT;
    else if(current_roll < -ANGLE_LIMIT) current_roll = -ANGLE_LIMIT;
    
    if(current_pitch > ANGLE_LIMIT) current_pitch = ANGLE_LIMIT;
    else if(current_pitch < -ANGLE_LIMIT) current_pitch = -ANGLE_LIMIT;

    /* 角度外环：目标角度与当前角度的误差，经 P 控制转为目标角速度 */
    float roll_angle_err  =  target_roll - current_roll;
    float pitch_angle_err = target_pitch - current_pitch;
    Pitch_err = pitch_angle_err;
    
    /* Yaw 角跨 0/360 度处理，保证走最短角度误差 */
    yaw_err = target_yaw - current_yaw;
    if(yaw_err > 180.0f)  yaw_err -= 360.0f;
    if(yaw_err < -180.0f) yaw_err += 360.0f;

    /* 外环输出目标角速度，缓存给内环 */
    target_roll_rate  = Roll_Outer_Kp * roll_angle_err;
    target_pitch_rate = Pitch_Outer_Kp * pitch_angle_err;
    target_yaw_rate   = Yaw_Outer_Kp * yaw_err;
}

/**
 * @brief  角速度内环控制函数，高频调用，例如 500Hz/2ms。
 * @param  rollRate/pitchRate/yawRate 当前陀螺仪角速度。
 * @note   根据外环输出的目标角速度计算 PID 控制量，并更新混控输出。
 */
void Drone_Inner_Rate_PID_Control(float rollRate, float pitchRate, float yawRate)
{
    /* 角速度低通滤波 */
    float filtered_rollRate  = GYRO_LPF_ALPHA * rollRate  + (1 - GYRO_LPF_ALPHA) * last_rollRate;
    float filtered_pitchRate = GYRO_LPF_ALPHA * pitchRate + (1 - GYRO_LPF_ALPHA) * last_pitchRate;
    float filtered_yawRate   = GYRO_LPF_ALPHA * yawRate   + (1 - GYRO_LPF_ALPHA) * last_yawRate;

    last_rollRate  = filtered_rollRate;
    last_pitchRate = filtered_pitchRate;
    last_yawRate   = filtered_yawRate;

    /* 停机保护：油门很低时清空控制输出和积分，避免再次启动时积分残留 */
    if(BASE_DUTY <= 1)
    {
        pitch_pid_out = 0;
        yaw_pid_out = 0;
        roll_pid_out = 0;
        roll_rate_integral = 0; 
        pitch_rate_integral = 0; 
        yaw_rate_integral = 0;
    }
    else
    {
        /* Roll 内环 */
        float roll_rate_err = filtered_rollRate - target_roll_rate;
        
        /* P 项 */
        float roll_inner_p = Roll_Inner_Kp * roll_rate_err;
        
        /* I 项：误差穿越 0 时清空积分，降低过冲和震荡 */
        roll_rate_integral += Roll_Inner_Ki * roll_rate_err * PID_SAMPLE_TIME;
        if((roll_rate_err > 0 && roll_rate_integral < 0) ||
           (roll_rate_err < 0 && roll_rate_integral > 0))
            roll_rate_integral = 0;
        if(roll_rate_integral > Roll_Inner_I_Limit) roll_rate_integral = Roll_Inner_I_Limit;
        else if(roll_rate_integral < -Roll_Inner_I_Limit) roll_rate_integral = -Roll_Inner_I_Limit;
        
        /* D 项：基于陀螺仪测量值差分，而不是误差差分 */
        float roll_d_raw = Roll_Inner_Kd * (last_roll_rate - filtered_rollRate) / PID_SAMPLE_TIME;
        last_roll_rate = filtered_rollRate;
        roll_d_filtered = DTERM_LPF_ALPHA * roll_d_raw + (1.0f - DTERM_LPF_ALPHA) * roll_d_filtered;
        
        roll_pid_out = roll_inner_p + roll_rate_integral + roll_d_filtered;

        /* Pitch 内环 */
        float pitch_rate_err = filtered_pitchRate - target_pitch_rate;
        
        float pitch_inner_p = Pitch_Inner_Kp * pitch_rate_err;
        
        pitch_rate_integral += Pitch_Inner_Ki * pitch_rate_err * PID_SAMPLE_TIME;
        if((pitch_rate_err > 0 && pitch_rate_integral < 0) ||
           (pitch_rate_err < 0 && pitch_rate_integral > 0))
            pitch_rate_integral = 0;
        if(pitch_rate_integral > Pitch_Inner_I_Limit) pitch_rate_integral = Pitch_Inner_I_Limit;
        else if(pitch_rate_integral < -Pitch_Inner_I_Limit) pitch_rate_integral = -Pitch_Inner_I_Limit;
        
        float pitch_d_raw = Pitch_Inner_Kd * (last_pitch_rate - filtered_pitchRate) / PID_SAMPLE_TIME;
        last_pitch_rate = filtered_pitchRate;
        pitch_d_filtered = DTERM_LPF_ALPHA * pitch_d_raw + (1.0f - DTERM_LPF_ALPHA) * pitch_d_filtered;
        
        pitch_pid_out = pitch_inner_p + pitch_rate_integral + pitch_d_filtered;

        /* Yaw 内环。
         * 注意：Yaw 误差 = target - measured，与 Roll/Pitch 的 measured - target 相反。
         * 这是为了匹配 yaw 混控方向：yaw_pid_out 对 FL/BR 为正，对 FR/BL 为负。
         */
        float yaw_rate_err = target_yaw_rate - filtered_yawRate;
        
        float yaw_inner_p = Yaw_Inner_Kp * yaw_rate_err;
        
        yaw_rate_integral += Yaw_Inner_Ki * yaw_rate_err * PID_SAMPLE_TIME;
        if((yaw_rate_err > 0 && yaw_rate_integral < 0) ||
           (yaw_rate_err < 0 && yaw_rate_integral > 0))
            yaw_rate_integral = 0;
        if(yaw_rate_integral > Yaw_Inner_I_Limit) yaw_rate_integral = Yaw_Inner_I_Limit;
        else if(yaw_rate_integral < -Yaw_Inner_I_Limit) yaw_rate_integral = -Yaw_Inner_I_Limit;
        
        float yaw_d_raw = Yaw_Inner_Kd * (last_yaw_rate - filtered_yawRate) / PID_SAMPLE_TIME;
        last_yaw_rate = filtered_yawRate;
        yaw_d_filtered = DTERM_LPF_ALPHA * yaw_d_raw + (1.0f - DTERM_LPF_ALPHA) * yaw_d_filtered;
        
        yaw_pid_out = yaw_inner_p + yaw_rate_integral + yaw_d_filtered;
        yaw_ceshi = yaw_pid_out;
    }
    
    /* Motor mix output, with yaw correction enabled. */
	Motor_Duty_FrontLeft  = BASE_DUTY + pitch_pid_out - roll_pid_out + yaw_pid_out;
    Motor_Duty_FrontRight = BASE_DUTY + pitch_pid_out + roll_pid_out - yaw_pid_out;
    Motor_Duty_BackLeft   = BASE_DUTY - pitch_pid_out - roll_pid_out - yaw_pid_out;
    Motor_Duty_BackRight  = BASE_DUTY - pitch_pid_out + roll_pid_out + yaw_pid_out;

    /* 输出限幅 */
    if(Motor_Duty_FrontLeft > PWM_MAX) Motor_Duty_FrontLeft = PWM_MAX;
    else if(Motor_Duty_FrontLeft < PWM_MIN) Motor_Duty_FrontLeft = PWM_MIN;
    
    if(Motor_Duty_FrontRight > PWM_MAX) Motor_Duty_FrontRight = PWM_MAX;
    else if(Motor_Duty_FrontRight < PWM_MIN) Motor_Duty_FrontRight = PWM_MIN;
    
    if(Motor_Duty_BackLeft > PWM_MAX) Motor_Duty_BackLeft = PWM_MAX;
    else if(Motor_Duty_BackLeft < PWM_MIN) Motor_Duty_BackLeft = PWM_MIN;
    
    if(Motor_Duty_BackRight > PWM_MAX) Motor_Duty_BackRight = PWM_MAX;
    else if(Motor_Duty_BackRight < PWM_MIN) Motor_Duty_BackRight = PWM_MIN;
}

/* 获取电机 PWM 比较值：0~100% 映射为 500~1000 */
uint16_t Get_Motor_Duty_FrontLeft(void)  { return (uint16_t)(500.0f + Motor_Duty_FrontLeft  * 5.0f); }
uint16_t Get_Motor_Duty_FrontRight(void) { return (uint16_t)(500.0f + Motor_Duty_FrontRight * 5.0f); }
uint16_t Get_Motor_Duty_BackLeft(void)   { return (uint16_t)(500.0f + Motor_Duty_BackLeft   * 5.0f); }
uint16_t Get_Motor_Duty_BackRight(void)  { return (uint16_t)(500.0f + Motor_Duty_BackRight  * 5.0f); }

/* 重置 PID 积分和 D 项历史值 */
void Drone_PID_Reset(void)
{
    roll_rate_integral  = 0.0f;
    pitch_rate_integral = 0.0f;
    yaw_rate_integral   = 0.0f;
    last_roll_rate      = 0.0f;
    last_pitch_rate     = 0.0f;
    last_yaw_rate       = 0.0f;
    roll_d_filtered     = 0.0f;
    pitch_d_filtered    = 0.0f;
    yaw_d_filtered      = 0.0f;
    altitude_integral   = 0.0f;
    position_x_integral = 0.0f;
    position_y_integral = 0.0f;
    last_altitude_err   = 0.0f;
    last_position_x_err = 0.0f;
    last_position_y_err = 0.0f;
    altitude_pid_out    = 0.0f;
    position_roll_aim   = 0.0f;
    position_pitch_aim  = 0.0f;
}



/* 兼容旧调参接口。
 * 当前已经改为串级 PID，旧函数默认映射到内环角速度参数。
 */

void Pitch_Kp_Get(float p) { Pitch_Inner_Kp = p; }
void Pitch_Ki_Get(float p) { Pitch_Inner_Ki = p; }
void Pitch_Kd_Get(float d) { Pitch_Inner_Kd = d; }

void Roll_Kp_Get(float p)  { Roll_Inner_Kp = p; }
void Roll_Ki_Get(float p)  { Roll_Inner_Ki = p; }
void Roll_Kd_Get(float d)  { Roll_Inner_Kd = d; }

void Yaw_Kp_Get(float p)   { Yaw_Inner_Kp = p; }
void Yaw_Ki_Get(float p)   { Yaw_Inner_Ki = p; }
void Yaw_Kd_Get(float d)   { Yaw_Inner_Kd = d; }

void Pitch_Angle_Kp_Get(float p) { Pitch_Outer_Kp = p; }
void Roll_Angle_Kp_Get(float p) { Roll_Outer_Kp = p; }
void Yaw_Angle_Kp_Get(float p) { Yaw_Outer_Kp = p; }
