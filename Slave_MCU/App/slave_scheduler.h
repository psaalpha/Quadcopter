#ifndef SLAVE_SCHEDULER_H
#define SLAVE_SCHEDULER_H
#include "stm32f10x.h"
typedef enum {
    SLAVE_TASK_SENSORS = 0,
    SLAVE_TASK_DISPLAY,
    SLAVE_TASK_COUNT
} SlaveTaskId;
void SlaveScheduler_Init(void);
void SlaveScheduler_TickFromIsr(uint32_t elapsed_ms);
uint32_t SlaveScheduler_Now(void);
uint8_t SlaveScheduler_Take(SlaveTaskId task);
uint32_t SlaveScheduler_GetOverruns(SlaveTaskId task);
#endif
