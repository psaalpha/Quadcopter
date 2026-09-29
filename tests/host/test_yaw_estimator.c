#include "hubu.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

/* hubu.c 的 IMU/Kalman 依赖只为链接提供桩；本测试只调用磁航向入口。 */
void MPU6050_GetData(int16_t *ax, int16_t *ay, int16_t *az,
                     int16_t *gx, int16_t *gy, int16_t *gz)
{
    *ax = *ay = *az = *gx = *gy = *gz = 0;
}

float Kalman_Get_Roll(float gyro, float angle, float dt)
{
    (void)gyro; (void)dt;
    return angle;
}

float Kalman_Get_Pitch(float gyro, float angle, float dt)
{
    (void)gyro; (void)dt;
    return angle;
}

int main(void)
{
    float roll;
    float pitch;
    float yaw;

    Yaw_Calibrate();
    assert(Yaw_ApplyMagHeading(350.0f) == 1u);
    Get_Angle(&roll, &pitch, &yaw);
    assert(yaw == 350.0f);

    /* 350°→10° 的最短差是 +20°；0.12 权重后应向 360° 靠近。 */
    assert(Yaw_ApplyMagHeading(10.0f) == 0u);
    Get_Angle(&roll, &pitch, &yaw);
    assert(yaw > 352.0f && yaw < 353.0f);

    assert(Yaw_ApplyMagHeading(-1.0f) == 0u);
    Get_Angle(&roll, &pitch, &yaw);
    assert(yaw > 352.0f && yaw < 353.0f);
    puts("yaw_estimator_test: PASS");
    return 0;
}
