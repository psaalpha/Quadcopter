#ifndef PERIODIC_TASKS_H
#define PERIODIC_TASKS_H
#include <stdint.h>
#define PERIODIC_TASK_LIMIT 4u
typedef struct {
    volatile uint32_t now_ms;
    uint32_t period_ms[PERIODIC_TASK_LIMIT];
    uint32_t remaining_ms[PERIODIC_TASK_LIMIT];
    volatile uint32_t overruns[PERIODIC_TASK_LIMIT];
    volatile uint8_t pending[PERIODIC_TASK_LIMIT];
    uint8_t count;
} PeriodicTasks;
void PeriodicTasks_Init(PeriodicTasks *tasks, const uint32_t *periods, uint8_t count);
/* Single ISR producer; coalesce overdue work instead of replaying it. */
void PeriodicTasks_Advance(PeriodicTasks *tasks, uint32_t elapsed_ms);
/* Caller masks interrupts while taking an event, before running the task. */
uint8_t PeriodicTasks_Take(PeriodicTasks *tasks, uint8_t index);
#endif
