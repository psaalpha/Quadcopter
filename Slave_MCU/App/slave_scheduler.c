#include "slave_scheduler.h"
#include "periodic_tasks.h"
#include "slave_board.h"
static periodic_tasks_t tasks;
void slave_scheduler_init(void)
{
    static const uint32_t periods[SLAVE_TASK_COUNT] = {
        SLAVE_SENSOR_PERIOD_MS, SLAVE_DISPLAY_PERIOD_MS
    };
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    periodic_tasks_init(&tasks, periods, SLAVE_TASK_COUNT);
    __set_PRIMASK(mask);
}
void slave_scheduler_tick_from_isr(uint32_t elapsed_ms)
{
    periodic_tasks_advance(&tasks, elapsed_ms);
}
uint32_t slave_scheduler_now_ms(void) { return tasks.now_ms; }
uint8_t slave_scheduler_take(slave_task_id_t task)
{
    uint8_t available;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    available = periodic_tasks_take(&tasks, (uint8_t)task);
    __set_PRIMASK(mask);
    return available;
}
uint32_t slave_scheduler_get_overruns(slave_task_id_t task)
{
    return (uint8_t)task < SLAVE_TASK_COUNT ? tasks.overruns[task] : 0u;
}
