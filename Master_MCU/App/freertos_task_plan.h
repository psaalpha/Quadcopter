#ifndef FREERTOS_TASK_PLAN_H
#define FREERTOS_TASK_PLAN_H

#include <stdint.h>

typedef enum
{
    RTOS_PLANNED_TASK_SENSOR = 0,
    RTOS_PLANNED_TASK_CONTROL,
    RTOS_PLANNED_TASK_COMMUNICATION,
    RTOS_PLANNED_TASK_SAFETY,
    RTOS_PLANNED_TASK_LOGGER,
    RTOS_PLANNED_TASK_COUNT
} RtosPlannedTaskId;

typedef enum
{
    RTOS_TASK_RELEASE_PERIODIC = 0,
    RTOS_TASK_RELEASE_NOTIFICATION,
    RTOS_TASK_RELEASE_PERIODIC_AND_EVENT
} RtosTaskReleaseMode;

typedef struct
{
    RtosPlannedTaskId id;
    const char *name;
    uint16_t period_ms;
    uint16_t deadline_ms;
    uint16_t execution_budget_us;
    uint16_t stack_words;
    uint8_t priority;
    RtosTaskReleaseMode release_mode;
} RtosTaskPlanSpec;

const RtosTaskPlanSpec *RtosTaskPlan_Get(RtosPlannedTaskId task);
uint8_t RtosTaskPlan_Count(void);
uint32_t RtosTaskPlan_TotalStackWords(void);
uint8_t RtosTaskPlan_Validate(void);

#endif
