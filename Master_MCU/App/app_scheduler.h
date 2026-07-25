#ifndef APP_SCHEDULER_H
#define APP_SCHEDULER_H

#include "stm32f10x.h"

typedef enum
{
    APP_TASK_IMU_UPDATE = 0,
    APP_TASK_RC_SERVICE,
    APP_TASK_ANGLE_CONTROL,
    APP_TASK_MOTOR_OUTPUT,
    APP_TASK_COUNT
} AppTaskId;

void AppScheduler_Init(void);
void AppScheduler_NotifyFromIsr(AppTaskId task);
uint8_t AppScheduler_Take(AppTaskId task);
uint8_t AppScheduler_GetPending(AppTaskId task);
uint32_t AppScheduler_GetOverrunCount(AppTaskId task);

#endif
