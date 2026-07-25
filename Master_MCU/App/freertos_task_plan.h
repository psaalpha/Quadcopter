/**
 * @file freertos_task_plan.h
 * @brief Non-executing five-task contract for a staged FreeRTOS migration.
 *
 * This module contains no FreeRTOS dependency and does not create tasks.
 */
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

/** @brief Return the immutable specification for one planned task. */
const RtosTaskPlanSpec *RtosTaskPlan_Get(RtosPlannedTaskId task);
/** @brief Return the number of task specifications in the plan. */
uint8_t RtosTaskPlan_Count(void);
/** @brief Return the initial sum of task stack budgets in 32-bit words. */
uint32_t RtosTaskPlan_TotalStackWords(void);
/** @brief Check task IDs, timing, stack, priority, and ordering invariants. */
uint8_t RtosTaskPlan_Validate(void);

#endif
