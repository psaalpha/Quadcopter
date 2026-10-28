#include "attitude_estimator.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
extern float estimated_roll_deg,estimated_pitch_deg,roll_mount_offset_deg,pitch_mount_offset_deg;

static int16_t test_gx=-64;
void mpu6050_get_data(int16_t *ax, int16_t *ay, int16_t *az,
                     int16_t *gx, int16_t *gy, int16_t *gz)
{
    *ax=*ay=0;*az=2048;*gx=test_gx;*gy=-29;*gz=-43;
}

uint8_t mpu6050_last_read_ok(void){return 1u;}

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
    roll_mount_offset_deg=pitch_mount_offset_deg=0;
    estimated_roll_deg=30;estimated_pitch_deg=20;
    attitude_estimator_reset_yaw();
    {
        float r=30*.01745329252f,p=20*.01745329252f;
        float x=1000*cosf(p)-500*sinf(p);
        float y=sinf(r)*(1000*sinf(p)+500*cosf(p));
        float z=cosf(r)*(1000*sinf(p)+500*cosf(p));
        assert(attitude_estimator_apply_mag_vector(x,y,z)==1u);
        attitude_estimator_get_angles(&roll,&pitch,&yaw);assert(yaw<.01f || yaw>359.99f);
        assert(attitude_estimator_apply_mag_vector(10000,0,0)==2u);
        assert(attitude_estimator_mag_rejections()>0u);
    }
    attitude_estimator_reset_yaw();attitude_estimator_apply_mag_heading(90);
    attitude_estimator_zero_heading();attitude_estimator_get_angles(&roll,&pitch,&yaw);assert(yaw==0);
    attitude_estimator_apply_mag_heading(100);attitude_estimator_get_angles(&roll,&pitch,&yaw);
    assert(yaw>1 && yaw<2);
    {
        unsigned i;float x,y,z;
        test_gx=1000;attitude_estimator_calibrate_start();
        for(i=0;i<2600;i++)attitude_estimator_update();
        assert(attitude_estimator_calibration_status()==3u);
        attitude_estimator_get_bias(&x,&y,&z);assert(x==-64);
        test_gx=-60;attitude_estimator_calibrate_start();
        for(i=0;i<256;i++)attitude_estimator_update();
        assert(attitude_estimator_calibration_status()==2u);
        attitude_estimator_get_bias(&x,&y,&z);assert(x==-60 && y==-29 && z==-43);
    }
    puts("yaw_estimator_test: PASS");
    return 0;
}
