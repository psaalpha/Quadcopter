#ifndef MASTER_MCU_CONTROL_ATTITUDE_ESTIMATOR_H
#define MASTER_MCU_CONTROL_ATTITUDE_ESTIMATOR_H

#include <stdint.h>

uint8_t attitude_estimator_update(void);
void attitude_estimator_set_bias(float x,float y,float z);
void attitude_estimator_get_bias(float *x,float *y,float *z);
void attitude_estimator_calibrate_start(void);
uint8_t attitude_estimator_calibration_status(void);
uint32_t attitude_estimator_mag_rejections(void);
uint8_t attitude_estimator_gravity_ok(void);
uint8_t attitude_estimator_apply_mag_vector(float x,float y,float z);
void attitude_estimator_get_angles(float *roll, float *pitch, float *yaw);
void attitude_estimator_get_rates(float *roll_rate, float *pitch_rate, float *yaw_rate);
/* 从控磁力计有效新帧到来时调用；首次对齐返回 1，供上层同步目标航向。 */
uint8_t attitude_estimator_apply_mag_heading(float heading_deg);
void attitude_estimator_reset_yaw(void);
void attitude_estimator_zero_heading(void);
#endif
