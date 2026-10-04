#include "slave_scheduler.h"
#include "periodic_tasks.h"
#include "slave_board.h"
static PeriodicTasks tasks;
void SlaveScheduler_Init(void)
{
    static const uint32_t periods[SLAVE_TASK_COUNT] = {
        SLAVE_SENSOR_PERIOD_MS, SLAVE_DISPLAY_PERIOD_MS
    };
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    PeriodicTasks_Init(&tasks, periods, SLAVE_TASK_COUNT);
    __set_PRIMASK(mask);
}
void SlaveScheduler_TickFromIsr(uint32_t elapsed_ms)
{
    PeriodicTasks_Advance(&tasks, elapsed_ms);
}
uint32_t SlaveScheduler_Now(void) { return tasks.now_ms; }
uint8_t SlaveScheduler_Take(SlaveTaskId task)
{
    uint8_t available;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    available = PeriodicTasks_Take(&tasks, (uint8_t)task);
    __set_PRIMASK(mask);
    return available;
}
uint32_t SlaveScheduler_GetOverruns(SlaveTaskId task)
{
    return (uint8_t)task < SLAVE_TASK_COUNT ? tasks.overruns[task] : 0u;
}
