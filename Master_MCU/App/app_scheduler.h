#ifndef APP_SCHEDULER_H
#define APP_SCHEDULER_H

#include <stdint.h>

#include "app_task_model.h"

void AppScheduler_Init(void);
void AppScheduler_NotifyFromIsr(AppTaskId task);
uint8_t AppScheduler_Take(AppTaskId task);
uint8_t AppScheduler_GetPending(AppTaskId task);
uint32_t AppScheduler_GetOverrunCount(AppTaskId task);

#endif
