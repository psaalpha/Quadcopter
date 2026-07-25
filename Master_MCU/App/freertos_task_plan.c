#include "freertos_task_plan.h"

#define RTOS_TASK_PLAN_MIN_STACK_WORDS  128u
#define RTOS_TASK_PLAN_MAX_PRIORITY     6u

static const RtosTaskPlanSpec rtos_task_plan[RTOS_PLANNED_TASK_COUNT] =
{
    {
        RTOS_PLANNED_TASK_SENSOR,
        "Sensor",
        2u,
        1u,
        500u,
        256u,
        5u,
        RTOS_TASK_RELEASE_PERIODIC
    },
    {
        RTOS_PLANNED_TASK_CONTROL,
        "Control",
        2u,
        2u,
        500u,
        320u,
        4u,
        RTOS_TASK_RELEASE_NOTIFICATION
    },
    {
        RTOS_PLANNED_TASK_COMMUNICATION,
        "Communication",
        5u,
        5u,
        600u,
        256u,
        3u,
        RTOS_TASK_RELEASE_PERIODIC_AND_EVENT
    },
    {
        RTOS_PLANNED_TASK_SAFETY,
        "Safety",
        5u,
        2u,
        250u,
        256u,
        6u,
        RTOS_TASK_RELEASE_PERIODIC
    },
    {
        RTOS_PLANNED_TASK_LOGGER,
        "Logger",
        20u,
        20u,
        800u,
        256u,
        1u,
        RTOS_TASK_RELEASE_PERIODIC_AND_EVENT
    }
};

const RtosTaskPlanSpec *RtosTaskPlan_Get(RtosPlannedTaskId task)
{
    uint8_t index = (uint8_t)task;

    if (index >= (uint8_t)RTOS_PLANNED_TASK_COUNT)
    {
        return 0;
    }
    return &rtos_task_plan[index];
}

uint8_t RtosTaskPlan_Count(void)
{
    return (uint8_t)RTOS_PLANNED_TASK_COUNT;
}

uint32_t RtosTaskPlan_TotalStackWords(void)
{
    uint8_t index;
    uint32_t total = 0u;

    for (index = 0u; index < (uint8_t)RTOS_PLANNED_TASK_COUNT; ++index)
    {
        total += rtos_task_plan[index].stack_words;
    }
    return total;
}

uint8_t RtosTaskPlan_Validate(void)
{
    uint8_t index;

    for (index = 0u; index < (uint8_t)RTOS_PLANNED_TASK_COUNT; ++index)
    {
        const RtosTaskPlanSpec *spec = &rtos_task_plan[index];

        if (((uint8_t)spec->id != index) || (spec->name == 0))
        {
            return 0u;
        }
        if ((spec->period_ms == 0u)
            || (spec->deadline_ms == 0u)
            || (spec->deadline_ms > spec->period_ms))
        {
            return 0u;
        }
        if ((spec->execution_budget_us == 0u)
            || ((uint32_t)spec->execution_budget_us
                >= ((uint32_t)spec->period_ms * 1000u)))
        {
            return 0u;
        }
        if (spec->stack_words < RTOS_TASK_PLAN_MIN_STACK_WORDS)
        {
            return 0u;
        }
        if ((spec->priority == 0u)
            || (spec->priority > RTOS_TASK_PLAN_MAX_PRIORITY))
        {
            return 0u;
        }
        if (spec->release_mode > RTOS_TASK_RELEASE_PERIODIC_AND_EVENT)
        {
            return 0u;
        }
    }

    if (rtos_task_plan[RTOS_PLANNED_TASK_SAFETY].priority
        <= rtos_task_plan[RTOS_PLANNED_TASK_CONTROL].priority)
    {
        return 0u;
    }
    if (rtos_task_plan[RTOS_PLANNED_TASK_SENSOR].priority
        <= rtos_task_plan[RTOS_PLANNED_TASK_CONTROL].priority)
    {
        return 0u;
    }
    if (rtos_task_plan[RTOS_PLANNED_TASK_LOGGER].priority
        >= rtos_task_plan[RTOS_PLANNED_TASK_COMMUNICATION].priority)
    {
        return 0u;
    }
    return 1u;
}
