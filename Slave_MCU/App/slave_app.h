#ifndef SLAVE_MCU_APP_SLAVE_APP_H
#define SLAVE_MCU_APP_SLAVE_APP_H
#include <stdint.h>
typedef struct {
    uint32_t calibration_saved;
    uint32_t calibration_errors;
    uint32_t barometer_errors;
    uint32_t display_restarts;
} slave_app_stats_t;
void slave_app_init(void);
void slave_app_process(void);
void slave_app_get_stats(slave_app_stats_t *stats);
#endif
