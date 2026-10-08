#ifndef SHARED_SCHEDULING_PERIODIC_TASKS_H
#define SHARED_SCHEDULING_PERIODIC_TASKS_H
#include <stdint.h>
#define PERIODIC_TASK_LIMIT 4u
typedef struct {
    volatile uint32_t now_ms;
    uint32_t period_ms[PERIODIC_TASK_LIMIT];
    uint32_t remaining_ms[PERIODIC_TASK_LIMIT];
    volatile uint32_t overruns[PERIODIC_TASK_LIMIT];
    volatile uint8_t pending[PERIODIC_TASK_LIMIT];
    uint8_t count;
} periodic_tasks_t;
void periodic_tasks_init(periodic_tasks_t *tasks, const uint32_t *periods, uint8_t count);
/* Single ISR producer; coalesce overdue work instead of replaying it. */
void periodic_tasks_advance(periodic_tasks_t *tasks, uint32_t elapsed_ms);
/* Caller masks interrupts while taking an event, before running the task. */
uint8_t periodic_tasks_take(periodic_tasks_t *tasks, uint8_t index);
#endif
