#include "attitude_estimator.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

/* attitude_estimator.c 的 IMU/Kalman 依赖只为链接提供桩；本测试只调用磁航向入口。 */
void mpu6050_get_data(int16_t *ax, int16_t *ay, int16_t *az,
                     int16_t *gx, int16_t *gy, int16_t *gz)
{
    *ax = *ay = *az = *gx = *gy = *gz = 0;
}

float kalman_update_roll(float gyro, float angle, float dt)
{
    (void)gyro; (void)dt;
    return angle;
}

float kalman_update_pitch(float gyro, float angle, float dt)
{
    (void)gyro; (void)dt;
    return angle;
}

int main(void)
{
    float roll;
    float pitch;
    float yaw;

    attitude_estimator_reset_yaw();
    assert(attitude_estimator_apply_mag_heading(350.0f) == 1u);
    attitude_estimator_get_angles(&roll, &pitch, &yaw);
    assert(yaw == 350.0f);

    /* 350°→10° 的最短差是 +20°；0.12 权重后应向 360° 靠近。 */
    assert(attitude_estimator_apply_mag_heading(10.0f) == 0u);
    attitude_estimator_get_angles(&roll, &pitch, &yaw);
    assert(yaw > 352.0f && yaw < 353.0f);

    assert(attitude_estimator_apply_mag_heading(-1.0f) == 0u);
    attitude_estimator_get_angles(&roll, &pitch, &yaw);
    assert(yaw > 352.0f && yaw < 353.0f);
    puts("yaw_estimator_test: PASS");
    return 0;
}
