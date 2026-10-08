#include "periodic_tasks.h"
#include <string.h>
void periodic_tasks_init(periodic_tasks_t *tasks, const uint32_t *periods, uint8_t count)
{
    uint8_t i;
    memset(tasks, 0, sizeof(*tasks));
    tasks->count = count > PERIODIC_TASK_LIMIT ? PERIODIC_TASK_LIMIT : count;
    for (i = 0u; i < tasks->count; ++i) {
        tasks->period_ms[i] = periods[i] == 0u ? 1u : periods[i];
        tasks->remaining_ms[i] = tasks->period_ms[i];
    }
}
void periodic_tasks_advance(periodic_tasks_t *tasks, uint32_t elapsed_ms)
{
    uint8_t i;
    tasks->now_ms += elapsed_ms;
    for (i = 0u; i < tasks->count; ++i) {
        if (elapsed_ms >= tasks->remaining_ms[i]) {
            uint32_t late = elapsed_ms - tasks->remaining_ms[i];
            uint32_t due = 1u + late / tasks->period_ms[i];
            tasks->remaining_ms[i] = tasks->period_ms[i] - late % tasks->period_ms[i];
            tasks->overruns[i] += due - (tasks->pending[i] == 0u ? 1u : 0u);
            tasks->pending[i] = 1u;
        } else {
            tasks->remaining_ms[i] -= elapsed_ms;
        }
    }
}
uint8_t periodic_tasks_take(periodic_tasks_t *tasks, uint8_t index)
{
    uint8_t pending;
    if (index >= tasks->count) return 0u;
    pending = tasks->pending[index];
    tasks->pending[index] = 0u;
    return pending;
}
