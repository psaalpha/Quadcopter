#include "stm32f10x.h"
#include "kalman_filter.h"
#include "math.h"

/* 卡尔曼滤波参数 */
#define KF_Q 0.2f   /* 过程噪声：越小越信任陀螺仪积分 */
#define KF_R 0.2f   /* 观测噪声：越大越抑制加速度计振动 */

/* estimated_roll_deg 轴卡尔曼状态 */
static float roll_angle;
static float roll_bias;
static float roll_covariance[2][2];

/* estimated_pitch_deg 轴卡尔曼状态 */
static float pitch_angle;
static float pitch_bias;
static float pitch_covariance[2][2];

/* 初始化 estimated_roll_deg 轴卡尔曼滤波器。 */
void kalman_init_roll(void)
{
    roll_angle = 0.0f;
    roll_bias = 0.0f;
    roll_covariance[0][0] = 1.0f;
    roll_covariance[0][1] = 0.0f;
    roll_covariance[1][0] = 0.0f;
    roll_covariance[1][1] = 1.0f;
}

/* 初始化 estimated_pitch_deg 轴卡尔曼滤波器。 */
void kalman_init_pitch(void)
{
    pitch_angle = 0.0f;
    pitch_bias = 0.0f;
    pitch_covariance[0][0] = 1.0f;
    pitch_covariance[0][1] = 0.0f;
    pitch_covariance[1][0] = 0.0f;
    pitch_covariance[1][1] = 1.0f;
}

/* estimated_roll_deg 轴卡尔曼滤波：融合陀螺仪角速度和加速度计角度。 */
float kalman_update_roll(float gyro, float acc_angle, float dt)
{
    float gyro_rate = gyro - roll_bias;
    roll_angle += gyro_rate * dt;

    roll_covariance[0][0] += dt * (dt*roll_covariance[1][1] - roll_covariance[0][1] - roll_covariance[1][0] + KF_Q);
    roll_covariance[0][1] -= dt * roll_covariance[1][1];
    roll_covariance[1][0] -= dt * roll_covariance[1][1];
    roll_covariance[1][1] += KF_Q * dt;

    float innovation_variance = roll_covariance[0][0] + KF_R;
    float angle_gain = roll_covariance[0][0] / innovation_variance;
    float bias_gain = roll_covariance[1][0] / innovation_variance;

    float innovation = acc_angle - roll_angle;
    roll_angle += angle_gain * innovation;
    roll_bias  += bias_gain * innovation;

    float previous_angle_variance = roll_covariance[0][0];
    roll_covariance[0][0] -= angle_gain * previous_angle_variance;
    roll_covariance[0][1] -= angle_gain * roll_covariance[0][1];
    roll_covariance[1][0] -= bias_gain * previous_angle_variance;
    roll_covariance[1][1] -= bias_gain * roll_covariance[1][1];

    /* 限制协方差矩阵，防止数值发散。 */
    if(roll_covariance[0][0] > 10.0f) roll_covariance[0][0] = 10.0f;
    if(roll_covariance[1][1] > 10.0f) roll_covariance[1][1] = 10.0f;
    if(roll_covariance[0][0] < 0.0f)  roll_covariance[0][0] = 0.0f;
    if(roll_covariance[1][1] < 0.0f)  roll_covariance[1][1] = 0.0f;

    return roll_angle;
}

float kalman_update_pitch(float gyro, float acc_angle, float dt)
{
    float gyro_rate = gyro - pitch_bias;
    pitch_angle += gyro_rate * dt;

    pitch_covariance[0][0] += dt * (dt*pitch_covariance[1][1] - pitch_covariance[0][1] - pitch_covariance[1][0] + KF_Q);
    pitch_covariance[0][1] -= dt * pitch_covariance[1][1];
    pitch_covariance[1][0] -= dt * pitch_covariance[1][1];
    pitch_covariance[1][1] += KF_Q * dt;

    float innovation_variance = pitch_covariance[0][0] + KF_R;
    float angle_gain = pitch_covariance[0][0] / innovation_variance;
    float bias_gain = pitch_covariance[1][0] / innovation_variance;

    float innovation = acc_angle - pitch_angle;
    pitch_angle += angle_gain * innovation;
    pitch_bias  += bias_gain * innovation;

    float previous_angle_variance = pitch_covariance[0][0];
    pitch_covariance[0][0] -= angle_gain * previous_angle_variance;
    pitch_covariance[0][1] -= angle_gain * pitch_covariance[0][1];
    pitch_covariance[1][0] -= bias_gain * previous_angle_variance;
    pitch_covariance[1][1] -= bias_gain * pitch_covariance[1][1];

    /* 限制协方差矩阵，防止数值发散。 */
    if(pitch_covariance[0][0] > 10.0f) pitch_covariance[0][0] = 10.0f;
    if(pitch_covariance[1][1] > 10.0f) pitch_covariance[1][1] = 10.0f;
    if(pitch_covariance[0][0] < 0.0f)  pitch_covariance[0][0] = 0.0f;
    if(pitch_covariance[1][1] < 0.0f)  pitch_covariance[1][1] = 0.0f;

    return pitch_angle;
}
