#ifndef SLAVE_APP_H
#define SLAVE_APP_H
#include <stdint.h>
typedef struct {
    uint32_t calibration_saved;
    uint32_t calibration_errors;
    uint32_t barometer_errors;
    uint32_t display_restarts;
} SlaveAppStats;
void SlaveApp_Init(void);
void SlaveApp_Process(void);
void SlaveApp_GetStats(SlaveAppStats *stats);
#endif
