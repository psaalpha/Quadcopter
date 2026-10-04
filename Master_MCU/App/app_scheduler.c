#include "app_scheduler.h"
#include "board_config.h"
#include "periodic_tasks.h"
static PeriodicTasks tasks;
void AppScheduler_Init(void)
{
    static const uint32_t periods[APP_TASK_COUNT] = {
        BOARD_IMU_TASK_PERIOD_MS, BOARD_RC_TASK_PERIOD_MS,
        BOARD_ANGLE_TASK_PERIOD_MS, BOARD_TELEMETRY_TASK_PERIOD_MS
    };
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    PeriodicTasks_Init(&tasks, periods, APP_TASK_COUNT);
    __set_PRIMASK(mask);
}
void AppScheduler_TickFromIsr(uint32_t elapsed_ms)
{
    PeriodicTasks_Advance(&tasks, elapsed_ms);
}
uint8_t AppScheduler_Take(AppTaskId task)
{
    uint8_t available;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    available = PeriodicTasks_Take(&tasks, (uint8_t)task);
    __set_PRIMASK(mask);
    return available;
}
uint8_t AppScheduler_GetPending(AppTaskId task)
{
    return (uint8_t)task < APP_TASK_COUNT ? tasks.pending[task] : 0u;
}
uint32_t AppScheduler_GetOverrunCount(AppTaskId task)
{
    return (uint8_t)task < APP_TASK_COUNT ? tasks.overruns[task] : 0u;
}
