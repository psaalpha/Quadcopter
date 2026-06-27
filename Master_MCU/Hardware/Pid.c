#include "stm32f10x.h"
#include <math.h>
#include "hubu.h"                  // Device header

// ====================== 全局变量：四轴电机PWM占空比（0~100%） ======================
static float Motor_Duty_FrontLeft  = 0.0f;
static float Motor_Duty_FrontRight = 0.0f;
static float Motor_Duty_BackLeft   = 0.0f;
static float Motor_Duty_BackRight  = 0.0f;

// ====================== 四轴核心配置 ======================
static float BASE_DUTY = 0.0f;   
#define PWM_MAX            100.0f          
#define PWM_MIN             0.0f           
#define ANGLE_LIMIT        30.0f           
#define YAW_ANGLE_LIMIT    180.0f          
#define PID_SAMPLE_TIME    0.002f  // 2ms = 500Hz (内环频率)

// ==================== 角速度低通滤波系数 ====================
// hubu.c 中已用 GYRO_FILTER_ALPHA 对 gx/gy 做过滤波，此处不再重复
// 设为 1.0f 即透传，如需额外滤波可调回 <1.0
#define GYRO_LPF_ALPHA 1.0f
static float last_rollRate = 0;
static float last_pitchRate = 0;
static float last_yawRate = 0;

// ====================== 【外环】角度环PID参数（仅P控制） ======================
float Roll_Outer_Kp  =12.03f;   // 角度环P：决定响应快慢，太大震荡，太小迟钝
float Pitch_Outer_Kp = 6.03f;   
float Yaw_Outer_Kp   = 5.72f;   // Yaw角度环P通常比Roll/Pitch小

//外环输出的目标角速度
static float target_roll_rate  = 0.0f;
static float target_pitch_rate = 0.0f;
static float target_yaw_rate   = 0.0f;
// ====================== 【内环】角速度环PID参数（完整PID） ======================
// Roll 内环
float Roll_Inner_Kp = 0.80f;   // 角速度环P：核心参数，抵消外环输出的角速度误差
float Roll_Inner_Ki = 0.105f;  // 角速度环I：消除静态角速度误差（机械偏心）
float Roll_Inner_Kd = 0.0018f;  // 角速度环D：抑制角速度震荡
static float Roll_Inner_I_Limit = 5.0f;

// Pitch 内环
float Pitch_Inner_Kp = 0.179f;
float Pitch_Inner_Ki = 0.1058f;
float Pitch_Inner_Kd = 0.001838f;
static float Pitch_Inner_I_Limit = 5.0f;

// Yaw 内环
float Yaw_Inner_Kp = 2.87f;
float Yaw_Inner_Ki = 0.0251f;
float Yaw_Inner_Kd = 0.0034444f;
static float Yaw_Inner_I_Limit = 3.0f;

// ====================== D项低通滤波系数 ====================
// 0.0=最强滤波(延迟大), 1.0=无滤波。建议从0.3开始试，逐步提高
#define DTERM_LPF_ALPHA 0.3f

// ====================== PID缓存变量 ======================
// 内环积分
static float roll_rate_integral  = 0.0f;
static float pitch_rate_integral = 0.0f;
static float yaw_rate_integral   = 0.0f;

// D项：存储上一帧陀螺仪原始值（D on measurement, not error）
static float last_roll_rate   = 0.0f;
static float last_pitch_rate  = 0.0f;
static float last_yaw_rate    = 0.0f;

// D项低通滤波历史值
static float roll_d_filtered  = 0.0f;
static float pitch_d_filtered = 0.0f;
static float yaw_d_filtered   = 0.0f;

// 最终输出给混控的值
float roll_pid_out;
float pitch_pid_out;
float yaw_pid_out;

// 目标角度
float target_roll  = 0.0f;
float target_pitch = 0.0f;
float target_yaw   = 0.0f;

// 测试用
float yaw_err;
float yaw_ceshi = 0;
float Pitch_err;

// ====================== 参数设置函数 ======================
void Set_Base_Duty(float value) {
    if(value >= 0.0f && value <= 100.0f) BASE_DUTY = value;
}

// 外环参数设置
void Roll_Outer_Kp_Get(float p)  { Roll_Outer_Kp = p; }
void Pitch_Outer_Kp_Get(float p) { Pitch_Outer_Kp = p; }
void Yaw_Outer_Kp_Get(float p)   { Yaw_Outer_Kp = p; }

// 内环参数设置
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

float Pitch_err_Get(void) { return Pitch_err; }
float Yaw_err_Get(void) { return yaw_err; }
float Yaw_pid_Get(void) { return yaw_ceshi; }



void Drone_Outer_Angle_PID_Control(float current_roll, float current_pitch, float current_yaw)
{
    // ==================== 角度限幅 ====================
    if(current_roll > ANGLE_LIMIT) current_roll = ANGLE_LIMIT;
    else if(current_roll < -ANGLE_LIMIT) current_roll = -ANGLE_LIMIT;
    
    if(current_pitch > ANGLE_LIMIT) current_pitch = ANGLE_LIMIT;
    else if(current_pitch < -ANGLE_LIMIT) current_pitch = -ANGLE_LIMIT;

    // ==================== 角度环（纯P控制） ====================
    // 计算角度误差
    float roll_angle_err  =  target_roll - current_roll;
    float pitch_angle_err = target_pitch - current_pitch;
    Pitch_err = pitch_angle_err;
    
    // Yaw角跨零处理
    yaw_err = target_yaw - current_yaw;
    if(yaw_err > 180.0f)  yaw_err -= 360.0f;
    if(yaw_err < -180.0f) yaw_err += 360.0f;

    // 外环输出 = 目标角速度，缓存到全局变量供内环使用
    target_roll_rate  = Roll_Outer_Kp * roll_angle_err;
    target_pitch_rate = Pitch_Outer_Kp * pitch_angle_err;
    target_yaw_rate   = Yaw_Outer_Kp * yaw_err;
}

/**
 * @brief  【拆分后】内环：角速度环控制函数（高频调用，例如500Hz/2ms）
 * @param  rollRate/pitchRate/yawRate: 当前陀螺仪原始角速度数据
 * @note   该函数负责根据外环输出的目标角速度，计算电机控制量并更新PWM占空比
 */
void Drone_Inner_Rate_PID_Control(float rollRate, float pitchRate, float yawRate)
{
    // ==================== 角速度低通滤波（原有逻辑完全保留） ====================
    float filtered_rollRate  = GYRO_LPF_ALPHA * rollRate  + (1 - GYRO_LPF_ALPHA) * last_rollRate;
    float filtered_pitchRate = GYRO_LPF_ALPHA * pitchRate + (1 - GYRO_LPF_ALPHA) * last_pitchRate;
    float filtered_yawRate   = GYRO_LPF_ALPHA * yawRate   + (1 - GYRO_LPF_ALPHA) * last_yawRate;

    last_rollRate  = filtered_rollRate;
    last_pitchRate = filtered_pitchRate;
    last_yawRate   = filtered_yawRate;

    // ==================== 停机保护（原有逻辑完全保留） ====================
    if(BASE_DUTY <= 1)
    {
        pitch_pid_out = 0;
        yaw_pid_out = 0;
        roll_pid_out = 0;
        // 停机时清空积分，防止累积
        roll_rate_integral = 0; 
        pitch_rate_integral = 0; 
        yaw_rate_integral = 0;
    }
    else
    {
        // ==================== 内环：角速度环（PID控制） ====================
        // --- Roll 内环 ---
        float roll_rate_err = filtered_rollRate - target_roll_rate;
        
        // P项
        float roll_inner_p = Roll_Inner_Kp * roll_rate_err;
        
        // I项（积分分离：误差穿越0时清零积分，防震荡）
        roll_rate_integral += Roll_Inner_Ki * roll_rate_err * PID_SAMPLE_TIME;
        if((roll_rate_err > 0 && roll_rate_integral < 0) ||
           (roll_rate_err < 0 && roll_rate_integral > 0))
            roll_rate_integral = 0;  // 误差过零，清积分
        if(roll_rate_integral > Roll_Inner_I_Limit) roll_rate_integral = Roll_Inner_I_Limit;
        else if(roll_rate_integral < -Roll_Inner_I_Limit) roll_rate_integral = -Roll_Inner_I_Limit;
        
        // D项：基于陀螺仪测量值（而非误差），避免微分踢
        float roll_d_raw = Roll_Inner_Kd * (last_roll_rate - filtered_rollRate) / PID_SAMPLE_TIME;
        last_roll_rate = filtered_rollRate;
        // D项一阶低通滤波
        roll_d_filtered = DTERM_LPF_ALPHA * roll_d_raw + (1.0f - DTERM_LPF_ALPHA) * roll_d_filtered;
        
        roll_pid_out = roll_inner_p + roll_rate_integral + roll_d_filtered;

        // --- Pitch 内环 ---
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

        // --- Yaw 内环 ---
        // 注意：Yaw 误差 = target - measured，与 Roll/Pitch 的 measured - target 相反
        // 这是因为混控公式中 yaw_pid_out 对 FL+BR 是 +、对 FR+BL 是 -
        // 当 drone 右转(正 yawRate)需抑制时，target=0, error=-filtered → yaw_pid_out 为负 → FL+BR 减速 ✓
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
    
    // ==================== 电机混控输出 ====================
	Motor_Duty_FrontLeft  = BASE_DUTY + pitch_pid_out - roll_pid_out;// + yaw_pid_out		  //0到100范围
    Motor_Duty_FrontRight = BASE_DUTY + pitch_pid_out + roll_pid_out;// - yaw_pid_out         //
    Motor_Duty_BackLeft   = BASE_DUTY - pitch_pid_out - roll_pid_out;// - yaw_pid_out         //
    Motor_Duty_BackRight  = BASE_DUTY - pitch_pid_out + roll_pid_out;// + yaw_pid_out         //

    // ==================== PWM占空比限幅100 ====================
    if(Motor_Duty_FrontLeft > PWM_MAX) Motor_Duty_FrontLeft = PWM_MAX;
    else if(Motor_Duty_FrontLeft < PWM_MIN) Motor_Duty_FrontLeft = PWM_MIN;
    
    if(Motor_Duty_FrontRight > PWM_MAX) Motor_Duty_FrontRight = PWM_MAX;
    else if(Motor_Duty_FrontRight < PWM_MIN) Motor_Duty_FrontRight = PWM_MIN;
    
    if(Motor_Duty_BackLeft > PWM_MAX) Motor_Duty_BackLeft = PWM_MAX;
    else if(Motor_Duty_BackLeft < PWM_MIN) Motor_Duty_BackLeft = PWM_MIN;
    
    if(Motor_Duty_BackRight > PWM_MAX) Motor_Duty_BackRight = PWM_MAX;
    else if(Motor_Duty_BackRight < PWM_MIN) Motor_Duty_BackRight = PWM_MIN;
}

// ====================== 获取电机占空比（映射 0~100 → 500~1000） ======================
//
uint16_t Get_Motor_Duty_FrontLeft(void)  { return (uint16_t)(500.0f + Motor_Duty_FrontLeft  * 5.0f); }
uint16_t Get_Motor_Duty_FrontRight(void) { return (uint16_t)(500.0f + Motor_Duty_FrontRight * 5.0f); }
uint16_t Get_Motor_Duty_BackLeft(void)   { return (uint16_t)(500.0f + Motor_Duty_BackLeft   * 5.0f); }
uint16_t Get_Motor_Duty_BackRight(void)  { return (uint16_t)(500.0f + Motor_Duty_BackRight  * 5.0f); }

// ====================== 重置PID积分 ======================
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
}



// ====================== 兼容旧代码的接口函数（解决 L6218E 报错） ======================
// 你的 main.c 中还在调用旧的单环函数名，这里补上定义。
// 注意：现在是串级PID，旧函数默认映射到【内环角速度】参数（可根据需要自行修改映射）

void Pitch_Kp_Get(float p) { Pitch_Inner_Kp = p; } // 如果你希望遥控器调的是角度环，可改为 Pitch_Outer_Kp = p;
void Pitch_Ki_Get(float p) { Pitch_Inner_Ki = p; }
void Pitch_Kd_Get(float d) { Pitch_Inner_Kd = d; }

void Roll_Kp_Get(float p)  { Roll_Inner_Kp = p; }  // 如果你希望遥控器调的是角度环，可改为 Roll_Outer_Kp = p;
void Roll_Ki_Get(float p)  { Roll_Inner_Ki = p; }
void Roll_Kd_Get(float d)  { Roll_Inner_Kd = d; }

void Yaw_Kp_Get(float p)   { Yaw_Inner_Kp = p; }
void Yaw_Ki_Get(float p)   { Yaw_Inner_Ki = p; }
void Yaw_Kd_Get(float d)   { Yaw_Inner_Kd = d; }

void Pitch_Angle_Kp_Get(float p) { Pitch_Outer_Kp = p; }
void Roll_Angle_Kp_Get(float p) { Roll_Outer_Kp = p; }
void Yaw_Angle_Kp_Get(float p) { Yaw_Outer_Kp = p; }
