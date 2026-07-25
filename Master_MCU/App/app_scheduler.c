#include "app_scheduler.h"

#include "stm32f10x.h"

/*
 * Periodic control tasks are coalesced instead of replayed back-to-back.
 * A second notification while one is pending is a missed deadline and is
 * counted as an overrun.
 */
#define APP_TASK_PENDING_MAX  1u

static volatile uint8_t pending_tasks[APP_TASK_COUNT];
static volatile uint32_t task_overruns[APP_TASK_COUNT];

void AppScheduler_Init(void)
{
    uint8_t index;

    __disable_irq();
    for (index = 0u; index < (uint8_t)APP_TASK_COUNT; ++index)
    {
        pending_tasks[index] = 0u;
        task_overruns[index] = 0u;
    }
    __enable_irq();
}

void AppScheduler_NotifyFromIsr(AppTaskId task)
{
    uint8_t index = (uint8_t)task;

    if (index >= (uint8_t)APP_TASK_COUNT)
    {
        return;
    }

    if (pending_tasks[index] < APP_TASK_PENDING_MAX)
    {
        pending_tasks[index]++;
    }
    else
    {
        task_overruns[index]++;
    }
}

uint8_t AppScheduler_Take(AppTaskId task)
{
    uint8_t index = (uint8_t)task;
    uint8_t available = 0u;

    if (index >= (uint8_t)APP_TASK_COUNT)
    {
        return 0u;
    }

    __disable_irq();
    if (pending_tasks[index] != 0u)
    {
        pending_tasks[index]--;
        available = 1u;
    }
    __enable_irq();

    return available;
}

uint8_t AppScheduler_GetPending(AppTaskId task)
{
    uint8_t index = (uint8_t)task;

    if (index >= (uint8_t)APP_TASK_COUNT)
    {
        return 0u;
    }
    return pending_tasks[index];
}

uint32_t AppScheduler_GetOverrunCount(AppTaskId task)
{
    uint8_t index = (uint8_t)task;

    if (index >= (uint8_t)APP_TASK_COUNT)
    {
        return 0u;
    }
    return task_overruns[index];
}
