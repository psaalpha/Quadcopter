#ifndef MASTER_MCU_APP_APP_SCHEDULER_H
#define MASTER_MCU_APP_APP_SCHEDULER_H

#include "stm32f10x.h"

typedef enum
{
    APP_TASK_IMU_UPDATE = 0,
    APP_TASK_RC_SERVICE,
    APP_TASK_ANGLE_CONTROL,
    APP_TASK_TELEMETRY,
    APP_TASK_COUNT
} app_task_id_t;

void app_scheduler_init(void);
void app_scheduler_tick_from_isr(uint32_t elapsed_ms);
uint8_t app_scheduler_take(app_task_id_t task);
uint8_t app_scheduler_get_pending(app_task_id_t task);
uint32_t app_scheduler_get_overrun_count(app_task_id_t task);

#endif
