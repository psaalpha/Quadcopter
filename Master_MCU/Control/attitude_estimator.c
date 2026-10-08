#include "stm32f10x.h"
#include "mpu6050.h"
#include <math.h>
#include "kalman_filter.h"

/* 姿态解算参数 */
#define MAG_HEADING_CORRECTION_GAIN 0.12f          /* 每次有效磁力计新帧对 estimated_yaw_deg 估计的校正比例 */
#define ATTITUDE_SAMPLE_PERIOD_S 0.002f           /* TIM2 采样周期：2ms */
#define GYRO_SCALE 16.4f    /* 陀螺仪 ±2000dps：16.4 LSB/(deg/s) */
#define ACC_SCALE 1024.0f   /* 加速度计 ±16g：1024 LSB/g */
#define ATTITUDE_PI 3.14159265358979323846f
#define GYRO_FILTER_ALPHA 0.2f

/* 角速度一阶滤波历史值 */
static float last_gyro_x_dps = 0.0f;
static float last_gyro_y_dps = 0.0f;
static uint8_t mag_heading_initialized = 0u;

/* 姿态解算结果 */
float estimated_roll_deg=0.0f;
float estimated_pitch_deg = 0.0f;
float estimated_yaw_deg = 0.0f;

float gyro_x_dps=0.0f;
float gyro_y_dps=0.0f;
float gyro_z_dps=0.0f;

/* 机械安装误差补偿量，根据实机标定填写 */
float roll_mount_offset_deg=2.0f;
float pitch_mount_offset_deg =-1.0f;
float yaw_mount_offset_deg = +0.0f;

/* 航向角采用 [0,360)；做差时采用 [-180,180] 的最短转向。 */
static float attitude_wrap_360(float angle)
{
    while(angle >= 360.0f) angle -= 360.0f;
    while(angle < 0.0f) angle += 360.0f;
    return angle;
}

static float attitude_wrap_180(float angle)
{
    while(angle > 180.0f) angle -= 360.0f;
    while(angle < -180.0f) angle += 360.0f;
    return angle;
}

/* 姿态解算主函数：读取 MPU6050，并更新 estimated_roll_deg/estimated_pitch_deg/estimated_yaw_deg 与角速度。 */
void attitude_estimator_update(void)
{
    /* 读取 MPU6050 原始数据 */
    int16_t accel_x_raw, accel_y_raw, accel_z_raw, gyro_x_raw, gyro_y_raw, gyro_z_raw;
    mpu6050_get_data(&accel_x_raw, &accel_y_raw, &accel_z_raw, &gyro_x_raw, &gyro_y_raw, &gyro_z_raw);

    /* 用加速度计计算静态 estimated_roll_deg/estimated_pitch_deg 参考角 */
    float ax = (float)accel_x_raw / ACC_SCALE;
    float ay = (float)accel_y_raw / ACC_SCALE;
    float az = (float)accel_z_raw / ACC_SCALE;


    float roll_acc = atan2(ay, sqrt(ax*ax + az*az)) * 180/ATTITUDE_PI;
    float pitch_acc = atan2(-ax, sqrt(ay*ay + az*az)) * 180/ATTITUDE_PI;

    /* 陀螺仪零漂补偿，数值来自当前硬件标定 */
    gyro_x_raw += 64;
    gyro_y_raw += 29;
    gyro_z_raw +=43;

    /* 原始值换算为角速度，单位 deg/s */
     gyro_x_dps = (float)gyro_x_raw / GYRO_SCALE;
     gyro_y_dps = (float)gyro_y_raw / GYRO_SCALE;
     gyro_z_dps = (float)gyro_z_raw / GYRO_SCALE;
	/* 角速度一阶滤波 */
    gyro_x_dps = GYRO_FILTER_ALPHA * last_gyro_x_dps + (1 - GYRO_FILTER_ALPHA) * gyro_x_dps;
    gyro_y_dps = GYRO_FILTER_ALPHA * last_gyro_y_dps + (1 - GYRO_FILTER_ALPHA) * gyro_y_dps;
    last_gyro_x_dps = gyro_x_dps;
    last_gyro_y_dps = gyro_y_dps;

    /* 2ms 陀螺仪积分提供短时连续性；磁力计新帧在另一入口缓慢消除漂移。 */
    estimated_yaw_deg = attitude_wrap_360(estimated_yaw_deg + gyro_z_dps * ATTITUDE_SAMPLE_PERIOD_S);

    /* estimated_roll_deg/estimated_pitch_deg 使用卡尔曼滤波融合陀螺仪与加速度计 */
	estimated_roll_deg = kalman_update_roll(gyro_x_dps, roll_acc, 0.002f);
	estimated_pitch_deg = kalman_update_pitch(gyro_y_dps, pitch_acc, 0.002f);

}

/* 磁力计来自从控新数据，不在 500Hz IMU 任务内重复使用同一帧。
 * 首帧直接建立绝对航向基准；后续沿最短角误差做小比例校正。
 * 这里只做平面航向修正，尚无倾斜补偿或磁异常检测。
 */
uint8_t attitude_estimator_apply_mag_heading(float heading_deg)
{
    if(!((heading_deg >= 0.0f) && (heading_deg < 360.0f)))
    {
        return 0u;
    }

    if(mag_heading_initialized == 0u)
    {
        estimated_yaw_deg = heading_deg;
        mag_heading_initialized = 1u;
        return 1u;
    }

    estimated_yaw_deg = attitude_wrap_360(estimated_yaw_deg + MAG_HEADING_CORRECTION_GAIN * attitude_wrap_180(heading_deg - estimated_yaw_deg));
    return 0u;
}

/* 读取姿态角，并叠加安装误差补偿。 */
void attitude_estimator_get_angles(float *roll, float *pitch, float *yaw)
{
    *roll = estimated_roll_deg + roll_mount_offset_deg;
    *pitch = estimated_pitch_deg + pitch_mount_offset_deg;
    *yaw = attitude_wrap_360(estimated_yaw_deg + yaw_mount_offset_deg);
}

/* 获取三轴角速度。 */
void attitude_estimator_get_rates(float *roll_rate, float *pitch_rate, float *yaw_rate)
{
    *roll_rate  = gyro_x_dps;
    *pitch_rate = gyro_y_dps;
    *yaw_rate   =gyro_z_dps;
}

/* 手动清零 estimated_yaw_deg 积分角。 */
void attitude_estimator_reset_yaw(void)
{
    estimated_yaw_deg = 0.0f;
    mag_heading_initialized = 0u;
}
