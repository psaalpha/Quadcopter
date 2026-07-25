#ifndef APP_TASK_MODEL_H
#define APP_TASK_MODEL_H

#include <stdint.h>

typedef enum
{
    APP_TASK_IMU_UPDATE = 0,
    APP_TASK_RC_SERVICE,
    APP_TASK_ANGLE_CONTROL,
    APP_TASK_MOTOR_OUTPUT,
    APP_TASK_COUNT
} AppTaskId;

typedef enum
{
    APP_TASK_PRIORITY_BACKGROUND = 1,
    APP_TASK_PRIORITY_NORMAL,
    APP_TASK_PRIORITY_HIGH,
    APP_TASK_PRIORITY_CRITICAL
} AppTaskPriority;

typedef enum
{
    APP_TASK_TRIGGER_PERIODIC = 0,
    APP_TASK_TRIGGER_EVENT
} AppTaskTrigger;

typedef struct
{
    AppTaskId id;
    uint16_t period_ms;
    uint16_t deadline_ms;
    uint16_t rtos_stack_words;
    AppTaskPriority priority;
    AppTaskTrigger trigger;
    uint8_t coalesce_releases;
} AppTaskSpec;

#define APP_EXECUTION_MODEL_COOPERATIVE  0u
#define APP_EXECUTION_MODEL_FREERTOS     1u

#ifndef APP_EXECUTION_MODEL
#define APP_EXECUTION_MODEL APP_EXECUTION_MODEL_COOPERATIVE
#endif

#if APP_EXECUTION_MODEL != APP_EXECUTION_MODEL_COOPERATIVE
#error "FreeRTOS adapter is not enabled; keep the cooperative model selected"
#endif

const AppTaskSpec *AppTaskModel_Get(AppTaskId task);
uint8_t AppTaskModel_Validate(void);
uint32_t AppTaskModel_TotalStackWords(void);

#endif
