#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "freertos_task_plan.h"

int main(void)
{
    const RtosTaskPlanSpec *sensor;
    const RtosTaskPlanSpec *control;
    const RtosTaskPlanSpec *communication;
    const RtosTaskPlanSpec *safety;
    const RtosTaskPlanSpec *logger;

    assert(RtosTaskPlan_Validate() == 1u);
    assert(RtosTaskPlan_Count() == 5u);
    assert(RtosTaskPlan_TotalStackWords() == 1344u);
    assert(RtosTaskPlan_Get(RTOS_PLANNED_TASK_COUNT) == 0);

    sensor = RtosTaskPlan_Get(RTOS_PLANNED_TASK_SENSOR);
    control = RtosTaskPlan_Get(RTOS_PLANNED_TASK_CONTROL);
    communication = RtosTaskPlan_Get(RTOS_PLANNED_TASK_COMMUNICATION);
    safety = RtosTaskPlan_Get(RTOS_PLANNED_TASK_SAFETY);
    logger = RtosTaskPlan_Get(RTOS_PLANNED_TASK_LOGGER);

    assert(strcmp(sensor->name, "Sensor") == 0);
    assert(sensor->period_ms == 2u);
    assert(sensor->release_mode == RTOS_TASK_RELEASE_PERIODIC);
    assert(control->period_ms == 2u);
    assert(control->release_mode == RTOS_TASK_RELEASE_NOTIFICATION);
    assert(sensor->priority > control->priority);
    assert(communication->period_ms == 5u);
    assert(communication->release_mode
        == RTOS_TASK_RELEASE_PERIODIC_AND_EVENT);
    assert(safety->priority > sensor->priority);
    assert(safety->deadline_ms == 2u);
    assert(logger->period_ms == 20u);
    assert(logger->priority < communication->priority);
    return 0;
}
