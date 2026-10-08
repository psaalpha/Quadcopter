#include "app_scheduler.h"
#include "board_config.h"
#include "periodic_tasks.h"
static periodic_tasks_t tasks;
void app_scheduler_init(void)
{
    static const uint32_t periods[APP_TASK_COUNT] = {
        BOARD_IMU_TASK_PERIOD_MS, BOARD_RC_TASK_PERIOD_MS,
        BOARD_ANGLE_TASK_PERIOD_MS, BOARD_TELEMETRY_TASK_PERIOD_MS
    };
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    periodic_tasks_init(&tasks, periods, APP_TASK_COUNT);
    __set_PRIMASK(mask);
}
void app_scheduler_tick_from_isr(uint32_t elapsed_ms)
{
    periodic_tasks_advance(&tasks, elapsed_ms);
}
uint8_t app_scheduler_take(app_task_id_t task)
{
    uint8_t available;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    available = periodic_tasks_take(&tasks, (uint8_t)task);
    __set_PRIMASK(mask);
    return available;
}
uint8_t app_scheduler_get_pending(app_task_id_t task)
{
    return (uint8_t)task < APP_TASK_COUNT ? tasks.pending[task] : 0u;
}
uint32_t app_scheduler_get_overrun_count(app_task_id_t task)
{
    return (uint8_t)task < APP_TASK_COUNT ? tasks.overruns[task] : 0u;
}
