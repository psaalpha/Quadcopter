#ifndef MASTER_NAVIGATION_ESTIMATOR_H
#define MASTER_NAVIGATION_ESTIMATOR_H
#include <stdint.h>
typedef struct {
    int32_t last_x,last_y;
    uint32_t last_integration_us;
    uint16_t epoch;
    uint8_t initialized;
    float x_mm,y_mm,height_cm,velocity_x_cm_s,velocity_y_cm_s;
    float roll_integral,pitch_integral;
} navigation_estimator_t;
void navigation_estimator_reset(navigation_estimator_t *n);
void navigation_estimator_gyro(navigation_estimator_t *n,float roll_rate,float pitch_rate,float dt);
int navigation_estimator_update(navigation_estimator_t *n,int32_t x,int32_t y,uint32_t us,uint16_t epoch,
    uint16_t range_mm,float roll,float pitch,float yaw,float scale_x,float scale_y,float sensor_yaw);
#endif
