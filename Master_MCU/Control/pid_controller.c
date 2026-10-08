#include "pid_controller.h"

/* 四轴电机输出占空比，范围 0~100% */
static float motor_duty_front_left  = 0.0f;
static float motor_duty_front_right = 0.0f;
static float motor_duty_back_left   = 0.0f;
static float motor_duty_back_right  = 0.0f;

/* 控制器核心配置 */
static float base_duty = 0.0f;
#define PWM_MAX            100.0f
#define PWM_MIN             0.0f
#define ANGLE_LIMIT        30.0f
#define YAW_ANGLE_LIMIT    180.0f
#define PID_SAMPLE_TIME    0.002f  /* 2ms = 500Hz 内环频率 */

/* 角速度低通滤波系数。
 * attitude_estimator.c 中已对 gx/gy 做过滤波，这里默认透传；如需二次滤波可设为 < 1.0f。
 */
#define GYRO_LPF_ALPHA 1.0f
static float last_filtered_roll_rate = 0;
static float last_filtered_pitch_rate = 0;
static float last_filtered_yaw_rate = 0;

/* 外环：角度环参数，当前仅使用 P 控制 */
float roll_outer_kp  =12.03f;   /* 决定响应快慢：过大易震荡，过小响应迟钝 */
float pitch_outer_kp = 6.03f;
float yaw_outer_kp   = 5.72f;   /* estimated_yaw_deg 外环 P 通常小于 estimated_roll_deg/estimated_pitch_deg */

/* 外环输出的目标角速度，供内环使用 */
static float target_roll_rate  = 0.0f;
static float target_pitch_rate = 0.0f;
static float target_yaw_rate   = 0.0f;

/* 内环：角速度环 PID 参数 */
/* estimated_roll_deg 内环 */
float roll_inner_kp = 0.80f;    /* P：抵消目标角速度与实测角速度的误差 */
float roll_inner_ki = 0.105f;   /* I：消除机械偏心等造成的静态误差 */
float roll_inner_kd = 0.0018f;  /* D：抑制角速度震荡 */
static float roll_inner_i_limit = 5.0f;

/* estimated_pitch_deg 内环 */
float pitch_inner_kp = 0.179f;
float pitch_inner_ki = 0.1058f;
float pitch_inner_kd = 0.001838f;
static float pitch_inner_i_limit = 5.0f;

/* estimated_yaw_deg 内环 */
float yaw_inner_kp = 2.87f;
float yaw_inner_ki = 0.0251f;
float yaw_inner_kd = 0.0034444f;
static float yaw_inner_i_limit = 3.0f;

/* D 项低通滤波系数：0.0=滤波最强但延迟最大，1.0=不滤波。 */
#define DTERM_LPF_ALPHA 0.3f

/* 高度/光流环按从控新帧运行，实际间隔由调用者传入，不能假定总是 20ms。 */
#define NAV_PID_MAX_DT_S         0.15f
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
float yaw_angle_error;
float yaw_output_snapshot = 0;
float pitch_angle_error;

/* Altitude hold PID: output is throttle correction in duty percent. */
float altitude_kp = 0.0f;
float altitude_ki = 0.0f;
float altitude_kd = 0.0f;
static float target_altitude_cm = 0.0f;
static float altitude_integral = 0.0f;
static float last_altitude_err = 0.0f;
static float altitude_pid_out = 0.0f;

/* Optical-flow position PID: outputs are suggested angle corrections. */
float position_x_kp = 0.0f;
float position_x_ki = 0.0f;
float position_x_kd = 0.0f;
float position_y_kp = 0.0f;
float position_y_ki = 0.0f;
float position_y_kd = 0.0f;
static int32_t target_flow_x = 0;
static int32_t target_flow_y = 0;
static float position_x_integral = 0.0f;
static float position_y_integral = 0.0f;
static float last_position_x_err = 0.0f;
static float last_position_y_err = 0.0f;
static float position_roll_aim = 0.0f;
static float position_pitch_aim = 0.0f;
static uint8_t nav_derivative_ready = 0u;

static float pid_clamp_float(float value, float min, float max)
{
    if(value > max) return max;
    if(value < min) return min;
    return value;
}

/* 参数设置接口 */
void pid_set_base_duty(float value) {
    if(value >= 0.0f && value <= 100.0f) base_duty = value;
}

/* 外环参数设置 */
void pid_set_roll_outer_kp(float p)  { roll_outer_kp = p; }
void pid_set_pitch_outer_kp(float p) { pitch_outer_kp = p; }
void pid_set_yaw_outer_kp(float p)   { yaw_outer_kp = p; }

/* 内环参数设置 */
void pid_set_roll_rate_kp(float p)  { roll_inner_kp = p; }
void pid_set_roll_rate_ki(float p)  { roll_inner_ki = p; }
void pid_set_roll_rate_kd(float d)  { roll_inner_kd = d; }

void pid_set_pitch_rate_kp(float p) { pitch_inner_kp = p; }
void pid_set_pitch_rate_ki(float p) { pitch_inner_ki = p; }
void pid_set_pitch_rate_kd(float d) { pitch_inner_kd = d; }

void pid_set_yaw_rate_kp(float p)   { yaw_inner_kp = p; }
void pid_set_yaw_rate_ki(float p)   { yaw_inner_ki = p; }
void pid_set_yaw_rate_kd(float d)   { yaw_inner_kd = d; }

void pid_set_pitch_target(float d) { target_pitch = d; }
void pid_set_roll_target(float d)  { target_roll = d; }
void pid_set_yaw_target(float d)   { target_yaw = d; }
void pid_set_altitude_target(float d) { target_altitude_cm = d; }
void pid_set_position_target(int32_t x, int32_t y)
{
    target_flow_x = x;
    target_flow_y = y;
}

void pid_set_altitude_kp(float p) { altitude_kp = p; }
void pid_set_altitude_ki(float p) { altitude_ki = p; }
void pid_set_altitude_kd(float d) { altitude_kd = d; }
void pid_set_position_x_kp(float p) { position_x_kp = p; }
void pid_set_position_x_ki(float p) { position_x_ki = p; }
void pid_set_position_x_kd(float d) { position_x_kd = d; }
void pid_set_position_y_kp(float p) { position_y_kp = p; }
void pid_set_position_y_ki(float p) { position_y_ki = p; }
void pid_set_position_y_kd(float d) { position_y_kd = d; }

float pid_get_pitch_error(void) { return pitch_angle_error; }
float pid_get_yaw_error(void) { return yaw_angle_error; }
float pid_get_yaw_output_snapshot(void) { return yaw_output_snapshot; }
float pid_get_altitude_output(void) { return altitude_pid_out; }
float pid_get_position_roll_target(void) { return position_roll_aim; }
float pid_get_position_pitch_target(void) { return position_pitch_aim; }

void pid_reset_navigation(void)
{
    altitude_integral = 0.0f;
    position_x_integral = 0.0f;
    position_y_integral = 0.0f;
    last_altitude_err = 0.0f;
    last_position_x_err = 0.0f;
    last_position_y_err = 0.0f;
    altitude_pid_out = 0.0f;
    position_roll_aim = 0.0f;
    position_pitch_aim = 0.0f;
    nav_derivative_ready = 0u;
}

/* 高度输出单位为油门百分比；光流 X/Y 输出单位为姿态目标角（度）。
 * flow_x/y 仍是传感器原始量，未做相机尺度标定，不能当作米制位置。
 */
void pid_update_navigation(float current_altitude_cm,
                                         int32_t flow_x, int32_t flow_y,
                                         float dt_s)
{
    float altitude_d;
    float position_x_d;
    float position_y_d;

    /* 数据太久未更新时不沿用旧积分/微分，避免重新收到帧时突跳。 */
    if((dt_s <= 0.0f) || (dt_s > NAV_PID_MAX_DT_S) ||
       (base_duty <= 1.0f))
    {
        pid_reset_navigation();
        return;
    }

    float altitude_err = target_altitude_cm - current_altitude_cm;
    altitude_integral += altitude_ki * altitude_err * dt_s;
    altitude_integral = pid_clamp_float(altitude_integral, -ALTITUDE_I_LIMIT, ALTITUDE_I_LIMIT);

    altitude_d = nav_derivative_ready ?
        altitude_kd * (altitude_err - last_altitude_err) / dt_s : 0.0f;
    last_altitude_err = altitude_err;
    altitude_pid_out = altitude_kp * altitude_err + altitude_integral + altitude_d;
    altitude_pid_out = pid_clamp_float(altitude_pid_out, -ALTITUDE_OUT_LIMIT, ALTITUDE_OUT_LIMIT);

    /* 先转 float 再相减，避免两个 int32 原始积分量相减时发生有符号溢出。 */
    float position_x_err = (float)target_flow_x - (float)flow_x;
    position_x_integral += position_x_ki * position_x_err * dt_s;
    position_x_integral = pid_clamp_float(position_x_integral, -POSITION_I_LIMIT, POSITION_I_LIMIT);
    position_x_d = nav_derivative_ready ?
        position_x_kd * (position_x_err - last_position_x_err) / dt_s : 0.0f;
    last_position_x_err = position_x_err;
    position_roll_aim = position_x_kp * position_x_err + position_x_integral + position_x_d;
    position_roll_aim = pid_clamp_float(position_roll_aim, -POSITION_ANGLE_LIMIT, POSITION_ANGLE_LIMIT);

    float position_y_err = (float)target_flow_y - (float)flow_y;
    position_y_integral += position_y_ki * position_y_err * dt_s;
    position_y_integral = pid_clamp_float(position_y_integral, -POSITION_I_LIMIT, POSITION_I_LIMIT);
    position_y_d = nav_derivative_ready ?
        position_y_kd * (position_y_err - last_position_y_err) / dt_s : 0.0f;
    last_position_y_err = position_y_err;
    position_pitch_aim = position_y_kp * position_y_err + position_y_integral + position_y_d;
    position_pitch_aim = pid_clamp_float(position_pitch_aim, -POSITION_ANGLE_LIMIT, POSITION_ANGLE_LIMIT);

    nav_derivative_ready = 1u;
}



void pid_update_angle_loop(float current_roll, float current_pitch, float current_yaw)
{
    /* 角度测量限幅，避免异常姿态值直接放大到外环输出 */
    if(current_roll > ANGLE_LIMIT) current_roll = ANGLE_LIMIT;
    else if(current_roll < -ANGLE_LIMIT) current_roll = -ANGLE_LIMIT;

    if(current_pitch > ANGLE_LIMIT) current_pitch = ANGLE_LIMIT;
    else if(current_pitch < -ANGLE_LIMIT) current_pitch = -ANGLE_LIMIT;

    /* 角度外环：目标角度与当前角度的误差，经 P 控制转为目标角速度 */
    float roll_angle_err  =  target_roll - current_roll;
    float pitch_angle_err = target_pitch - current_pitch;
    pitch_angle_error = pitch_angle_err;

    /* estimated_yaw_deg 角跨 0/360 度处理，保证走最短角度误差 */
    yaw_angle_error = target_yaw - current_yaw;
    if(yaw_angle_error > 180.0f)  yaw_angle_error -= 360.0f;
    if(yaw_angle_error < -180.0f) yaw_angle_error += 360.0f;

    /* 外环输出目标角速度，缓存给内环 */
    target_roll_rate  = roll_outer_kp * roll_angle_err;
    target_pitch_rate = pitch_outer_kp * pitch_angle_err;
    target_yaw_rate   = yaw_outer_kp * yaw_angle_error;
}

/**
 * @brief  角速度内环控制函数，高频调用，例如 500Hz/2ms。
 * @param  roll_rate/pitch_rate/yaw_rate 当前陀螺仪角速度。
 * @note   根据外环输出的目标角速度计算 PID 控制量，并更新混控输出。
 */
void pid_update_rate_loop(float roll_rate, float pitch_rate, float yaw_rate)
{
    /* 角速度低通滤波 */
    float filtered_roll_rate  = GYRO_LPF_ALPHA * roll_rate  + (1 - GYRO_LPF_ALPHA) * last_filtered_roll_rate;
    float filtered_pitch_rate = GYRO_LPF_ALPHA * pitch_rate + (1 - GYRO_LPF_ALPHA) * last_filtered_pitch_rate;
    float filtered_yaw_rate   = GYRO_LPF_ALPHA * yaw_rate   + (1 - GYRO_LPF_ALPHA) * last_filtered_yaw_rate;

    last_filtered_roll_rate  = filtered_roll_rate;
    last_filtered_pitch_rate = filtered_pitch_rate;
    last_filtered_yaw_rate   = filtered_yaw_rate;

    /* 停机保护：油门很低时清空控制输出和积分，避免再次启动时积分残留 */
    if(base_duty <= 1)
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
        /* estimated_roll_deg 内环 */
        float roll_rate_err = filtered_roll_rate - target_roll_rate;

        /* P 项 */
        float roll_inner_p = roll_inner_kp * roll_rate_err;

        /* I 项：误差穿越 0 时清空积分，降低过冲和震荡 */
        roll_rate_integral += roll_inner_ki * roll_rate_err * PID_SAMPLE_TIME;
        if((roll_rate_err > 0 && roll_rate_integral < 0) ||
           (roll_rate_err < 0 && roll_rate_integral > 0))
            roll_rate_integral = 0;
        if(roll_rate_integral > roll_inner_i_limit) roll_rate_integral = roll_inner_i_limit;
        else if(roll_rate_integral < -roll_inner_i_limit) roll_rate_integral = -roll_inner_i_limit;

        /* D 项：基于陀螺仪测量值差分，而不是误差差分 */
        float roll_d_raw = roll_inner_kd * (last_roll_rate - filtered_roll_rate) / PID_SAMPLE_TIME;
        last_roll_rate = filtered_roll_rate;
        roll_d_filtered = DTERM_LPF_ALPHA * roll_d_raw + (1.0f - DTERM_LPF_ALPHA) * roll_d_filtered;

        roll_pid_out = roll_inner_p + roll_rate_integral + roll_d_filtered;

        /* estimated_pitch_deg 内环 */
        float pitch_rate_err = filtered_pitch_rate - target_pitch_rate;

        float pitch_inner_p = pitch_inner_kp * pitch_rate_err;

        pitch_rate_integral += pitch_inner_ki * pitch_rate_err * PID_SAMPLE_TIME;
        if((pitch_rate_err > 0 && pitch_rate_integral < 0) ||
           (pitch_rate_err < 0 && pitch_rate_integral > 0))
            pitch_rate_integral = 0;
        if(pitch_rate_integral > pitch_inner_i_limit) pitch_rate_integral = pitch_inner_i_limit;
        else if(pitch_rate_integral < -pitch_inner_i_limit) pitch_rate_integral = -pitch_inner_i_limit;

        float pitch_d_raw = pitch_inner_kd * (last_pitch_rate - filtered_pitch_rate) / PID_SAMPLE_TIME;
        last_pitch_rate = filtered_pitch_rate;
        pitch_d_filtered = DTERM_LPF_ALPHA * pitch_d_raw + (1.0f - DTERM_LPF_ALPHA) * pitch_d_filtered;

        pitch_pid_out = pitch_inner_p + pitch_rate_integral + pitch_d_filtered;

        /* estimated_yaw_deg 内环。
         * 注意：estimated_yaw_deg 误差 = target - measured，与 estimated_roll_deg/estimated_pitch_deg 的 measured - target 相反。
         * 这是为了匹配 yaw 混控方向：yaw_pid_out 对 FL/BR 为正，对 FR/BL 为负。
         */
        float yaw_rate_err = target_yaw_rate - filtered_yaw_rate;

        float yaw_inner_p = yaw_inner_kp * yaw_rate_err;

        yaw_rate_integral += yaw_inner_ki * yaw_rate_err * PID_SAMPLE_TIME;
        if((yaw_rate_err > 0 && yaw_rate_integral < 0) ||
           (yaw_rate_err < 0 && yaw_rate_integral > 0))
            yaw_rate_integral = 0;
        if(yaw_rate_integral > yaw_inner_i_limit) yaw_rate_integral = yaw_inner_i_limit;
        else if(yaw_rate_integral < -yaw_inner_i_limit) yaw_rate_integral = -yaw_inner_i_limit;

        float yaw_d_raw = yaw_inner_kd * (last_yaw_rate - filtered_yaw_rate) / PID_SAMPLE_TIME;
        last_yaw_rate = filtered_yaw_rate;
        yaw_d_filtered = DTERM_LPF_ALPHA * yaw_d_raw + (1.0f - DTERM_LPF_ALPHA) * yaw_d_filtered;

        yaw_pid_out = yaw_inner_p + yaw_rate_integral + yaw_d_filtered;
        yaw_output_snapshot = yaw_pid_out;
    }

    /* Motor mix output, with yaw correction enabled. */
	motor_duty_front_left  = base_duty + pitch_pid_out - roll_pid_out + yaw_pid_out;
    motor_duty_front_right = base_duty + pitch_pid_out + roll_pid_out - yaw_pid_out;
    motor_duty_back_left   = base_duty - pitch_pid_out - roll_pid_out - yaw_pid_out;
    motor_duty_back_right  = base_duty - pitch_pid_out + roll_pid_out + yaw_pid_out;

    /* 输出限幅 */
    if(motor_duty_front_left > PWM_MAX) motor_duty_front_left = PWM_MAX;
    else if(motor_duty_front_left < PWM_MIN) motor_duty_front_left = PWM_MIN;

    if(motor_duty_front_right > PWM_MAX) motor_duty_front_right = PWM_MAX;
    else if(motor_duty_front_right < PWM_MIN) motor_duty_front_right = PWM_MIN;

    if(motor_duty_back_left > PWM_MAX) motor_duty_back_left = PWM_MAX;
    else if(motor_duty_back_left < PWM_MIN) motor_duty_back_left = PWM_MIN;

    if(motor_duty_back_right > PWM_MAX) motor_duty_back_right = PWM_MAX;
    else if(motor_duty_back_right < PWM_MIN) motor_duty_back_right = PWM_MIN;
}

/* 获取电机 PWM 比较值：0~100% 映射为 500~1000 */
uint16_t pid_get_motor_compare_front_left(void)  { return (uint16_t)(500.0f + motor_duty_front_left  * 5.0f); }
uint16_t pid_get_motor_compare_front_right(void) { return (uint16_t)(500.0f + motor_duty_front_right * 5.0f); }
uint16_t pid_get_motor_compare_back_left(void)   { return (uint16_t)(500.0f + motor_duty_back_left   * 5.0f); }
uint16_t pid_get_motor_compare_back_right(void)  { return (uint16_t)(500.0f + motor_duty_back_right  * 5.0f); }

/* 重置 PID 积分和 D 项历史值 */
void pid_reset(void)
{
    roll_rate_integral  = 0.0f;
    pitch_rate_integral = 0.0f;
    yaw_rate_integral   = 0.0f;
    last_filtered_roll_rate       = 0.0f;
    last_filtered_pitch_rate      = 0.0f;
    last_filtered_yaw_rate        = 0.0f;
    last_roll_rate      = 0.0f;
    last_pitch_rate     = 0.0f;
    last_yaw_rate       = 0.0f;
    roll_d_filtered     = 0.0f;
    pitch_d_filtered    = 0.0f;
    yaw_d_filtered      = 0.0f;
    pid_reset_navigation();
    roll_pid_out        = 0.0f;
    pitch_pid_out       = 0.0f;
    yaw_pid_out         = 0.0f;
    yaw_output_snapshot           = 0.0f;
}

/* 立即清除基础油门、混控结果及所有控制器历史状态。 */
void pid_stop_motors(void)
{
    base_duty = 0.0f;
    motor_duty_front_left  = 0.0f;
    motor_duty_front_right = 0.0f;
    motor_duty_back_left   = 0.0f;
    motor_duty_back_right  = 0.0f;
    target_roll_rate  = 0.0f;
    target_pitch_rate = 0.0f;
    target_yaw_rate   = 0.0f;
    pid_reset();
}



/* 兼容旧调参接口。
 * 当前已经改为串级 PID，旧函数默认映射到内环角速度参数。
 */

void pid_set_pitch_kp(float p) { pitch_inner_kp = p; }
void pid_set_pitch_ki(float p) { pitch_inner_ki = p; }
void pid_set_pitch_kd(float d) { pitch_inner_kd = d; }

void pid_set_roll_kp(float p)  { roll_inner_kp = p; }
void pid_set_roll_ki(float p)  { roll_inner_ki = p; }
void pid_set_roll_kd(float d)  { roll_inner_kd = d; }

void pid_set_yaw_kp(float p)   { yaw_inner_kp = p; }
void pid_set_yaw_ki(float p)   { yaw_inner_ki = p; }
void pid_set_yaw_kd(float d)   { yaw_inner_kd = d; }

void pid_set_pitch_angle_kp(float p) { pitch_outer_kp = p; }
void pid_set_roll_angle_kp(float p) { roll_outer_kp = p; }
void pid_set_yaw_angle_kp(float p) { yaw_outer_kp = p; }
