#include "app_task_model.h"

#include "board_config.h"

#define APP_TASK_MIN_STACK_WORDS  128u

static const AppTaskSpec app_task_specs[APP_TASK_COUNT] =
{
    {
        APP_TASK_IMU_UPDATE,
        BOARD_IMU_TASK_PERIOD_MS,
        BOARD_IMU_TASK_PERIOD_MS,
        256u,
        APP_TASK_PRIORITY_CRITICAL,
        APP_TASK_TRIGGER_PERIODIC,
        1u
    },
    {
        APP_TASK_RC_SERVICE,
        BOARD_RC_TASK_PERIOD_MS,
        BOARD_RC_TASK_PERIOD_MS,
        192u,
        APP_TASK_PRIORITY_HIGH,
        APP_TASK_TRIGGER_PERIODIC,
        1u
    },
    {
        APP_TASK_ANGLE_CONTROL,
        BOARD_ANGLE_TASK_PERIOD_MS,
        BOARD_ANGLE_TASK_PERIOD_MS,
        192u,
        APP_TASK_PRIORITY_NORMAL,
        APP_TASK_TRIGGER_PERIODIC,
        1u
    },
    {
        APP_TASK_MOTOR_OUTPUT,
        BOARD_MOTOR_TASK_PERIOD_MS,
        BOARD_MOTOR_TASK_PERIOD_MS,
        192u,
        APP_TASK_PRIORITY_HIGH,
        APP_TASK_TRIGGER_PERIODIC,
        1u
    }
};

const AppTaskSpec *AppTaskModel_Get(AppTaskId task)
{
    uint8_t index = (uint8_t)task;

    if (index >= (uint8_t)APP_TASK_COUNT)
    {
        return 0;
    }

    return &app_task_specs[index];
}

uint8_t AppTaskModel_Validate(void)
{
    uint8_t index;

    for (index = 0u; index < (uint8_t)APP_TASK_COUNT; ++index)
    {
        const AppTaskSpec *spec = &app_task_specs[index];

        if ((uint8_t)spec->id != index)
        {
            return 0u;
        }
        if ((spec->period_ms == 0u)
            || (spec->deadline_ms == 0u)
            || (spec->deadline_ms > spec->period_ms))
        {
            return 0u;
        }
        if (spec->rtos_stack_words < APP_TASK_MIN_STACK_WORDS)
        {
            return 0u;
        }
        if ((spec->priority < APP_TASK_PRIORITY_BACKGROUND)
            || (spec->priority > APP_TASK_PRIORITY_CRITICAL))
        {
            return 0u;
        }
        if (spec->trigger != APP_TASK_TRIGGER_PERIODIC)
        {
            return 0u;
        }
        if (spec->coalesce_releases != 1u)
        {
            return 0u;
        }
    }

    return 1u;
}

uint32_t AppTaskModel_TotalStackWords(void)
{
    uint8_t index;
    uint32_t total = 0u;

    for (index = 0u; index < (uint8_t)APP_TASK_COUNT; ++index)
    {
        total += app_task_specs[index].rtos_stack_words;
    }

    return total;
}
