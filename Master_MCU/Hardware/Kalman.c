#include "stm32f10x.h"
#include "kalman.h"
#include "math.h"

/* 卡尔曼滤波参数 */
#define KF_Q 0.2f   /* 过程噪声：越小越信任陀螺仪积分 */
#define KF_R 0.2f   /* 观测噪声：越大越抑制加速度计振动 */

/* Roll 轴卡尔曼状态 */
static float roll_angle;
static float roll_bias;
static float P_roll[2][2];

/* Pitch 轴卡尔曼状态 */
static float pitch_angle;
static float pitch_bias;
static float P_pitch[2][2];

/* 初始化 Roll 轴卡尔曼滤波器。 */
void Kalman_Roll_Init(void)
{
    roll_angle = 0.0f;
    roll_bias = 0.0f;
    P_roll[0][0] = 1.0f;
    P_roll[0][1] = 0.0f;
    P_roll[1][0] = 0.0f;
    P_roll[1][1] = 1.0f;
}

/* 初始化 Pitch 轴卡尔曼滤波器。 */
void Kalman_Pitch_Init(void)
{
    pitch_angle = 0.0f;
    pitch_bias = 0.0f;
    P_pitch[0][0] = 1.0f;
    P_pitch[0][1] = 0.0f;
    P_pitch[1][0] = 0.0f;
    P_pitch[1][1] = 1.0f;
}

/* Roll 轴卡尔曼滤波：融合陀螺仪角速度和加速度计角度。 */
float Kalman_Get_Roll(float gyro, float acc_angle, float dt)
{
    float gyro_rate = gyro - roll_bias;
    roll_angle += gyro_rate * dt;

    P_roll[0][0] += dt * (dt*P_roll[1][1] - P_roll[0][1] - P_roll[1][0] + KF_Q);
    P_roll[0][1] -= dt * P_roll[1][1];
    P_roll[1][0] -= dt * P_roll[1][1];
    P_roll[1][1] += KF_Q * dt;

    float S = P_roll[0][0] + KF_R;
    float K0 = P_roll[0][0] / S;
    float K1 = P_roll[1][0] / S;

    float y = acc_angle - roll_angle;
    roll_angle += K0 * y;
    roll_bias  += K1 * y;

    float P00 = P_roll[0][0];
    P_roll[0][0] -= K0 * P00;
    P_roll[0][1] -= K0 * P_roll[0][1];
    P_roll[1][0] -= K1 * P00;
    P_roll[1][1] -= K1 * P_roll[1][1];

    /* 限制协方差矩阵，防止数值发散。 */
    if(P_roll[0][0] > 10.0f) P_roll[0][0] = 10.0f;
    if(P_roll[1][1] > 10.0f) P_roll[1][1] = 10.0f;
    if(P_roll[0][0] < 0.0f)  P_roll[0][0] = 0.0f;
    if(P_roll[1][1] < 0.0f)  P_roll[1][1] = 0.0f;

    return roll_angle;
}

float Kalman_Get_Pitch(float gyro, float acc_angle, float dt)
{
    float gyro_rate = gyro - pitch_bias;
    pitch_angle += gyro_rate * dt;

    P_pitch[0][0] += dt * (dt*P_pitch[1][1] - P_pitch[0][1] - P_pitch[1][0] + KF_Q);
    P_pitch[0][1] -= dt * P_pitch[1][1];
    P_pitch[1][0] -= dt * P_pitch[1][1];
    P_pitch[1][1] += KF_Q * dt;

    float S = P_pitch[0][0] + KF_R;
    float K0 = P_pitch[0][0] / S;
    float K1 = P_pitch[1][0] / S;

    float y = acc_angle - pitch_angle;
    pitch_angle += K0 * y;
    pitch_bias  += K1 * y;

    float P00 = P_pitch[0][0];
    P_pitch[0][0] -= K0 * P00;
    P_pitch[0][1] -= K0 * P_pitch[0][1];
    P_pitch[1][0] -= K1 * P00;
    P_pitch[1][1] -= K1 * P_pitch[1][1];

    /* 限制协方差矩阵，防止数值发散。 */
    if(P_pitch[0][0] > 10.0f) P_pitch[0][0] = 10.0f;
    if(P_pitch[1][1] > 10.0f) P_pitch[1][1] = 10.0f;
    if(P_pitch[0][0] < 0.0f)  P_pitch[0][0] = 0.0f;
    if(P_pitch[1][1] < 0.0f)  P_pitch[1][1] = 0.0f;

    return pitch_angle;
}
