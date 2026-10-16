#ifndef SLAVE_MCU_APP_SLAVE_APP_H
#define SLAVE_MCU_APP_SLAVE_APP_H
#include <stdint.h>
#include "optical_flow.h"
typedef struct {
    uint32_t calibration_saved;
    uint32_t calibration_errors;
    uint32_t barometer_errors;
    uint32_t display_restarts;
} slave_app_stats_t;
typedef struct {
    optical_flow_data_t flow;
    float yaw, temperature, battery, altitude_m;
    uint32_t calibration_remaining_ms;
    uint8_t baro_valid, calibrating;
} slave_display_snapshot_t;
void slave_app_capture_display(slave_display_snapshot_t *snapshot);
void slave_app_display_step(const slave_display_snapshot_t *snapshot);
/* 固件由slave_rtos实现静态队列；主机裸机回归不调用此接口。 */
void slave_rtos_submit_display(const slave_display_snapshot_t *snapshot);
void slave_app_init(void);
void slave_app_process(void);
void slave_app_get_stats(slave_app_stats_t *stats);
#endif
