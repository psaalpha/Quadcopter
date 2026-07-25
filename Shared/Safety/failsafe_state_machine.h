#ifndef FAILSAFE_STATE_MACHINE_H
#define FAILSAFE_STATE_MACHINE_H

#include <stdint.h>

typedef enum
{
    FAILSAFE_STATE_NORMAL = 0,
    FAILSAFE_STATE_WARNING,
    FAILSAFE_STATE_FAILSAFE,
    FAILSAFE_STATE_DISARM
} FailsafeState;

typedef enum
{
    FAILSAFE_EVENT_ARM_REQUEST = 0,
    FAILSAFE_EVENT_WARNING_PRESENT,
    FAILSAFE_EVENT_WARNINGS_CLEARED,
    FAILSAFE_EVENT_CRITICAL_FAULT,
    FAILSAFE_EVENT_ACTION_COMPLETE,
    FAILSAFE_EVENT_DISARM_REQUEST,
    FAILSAFE_EVENT_RESET
} FailsafeEvent;

typedef enum
{
    FAILSAFE_RESULT_NO_CHANGE = 0,
    FAILSAFE_RESULT_TRANSITIONED,
    FAILSAFE_RESULT_GUARD_REJECTED,
    FAILSAFE_RESULT_INVALID_ARGUMENT
} FailsafeResult;

typedef struct
{
    FailsafeState state;
    FailsafeState previous_state;
    FailsafeEvent last_event;
    uint32_t state_entered_ms;
    uint32_t transition_count;
    uint8_t initialized;
} FailsafeStateMachine;

void FailsafeStateMachine_Init(
    FailsafeStateMachine *machine,
    uint32_t timestamp_ms);
FailsafeResult FailsafeStateMachine_Handle(
    FailsafeStateMachine *machine,
    FailsafeEvent event,
    uint8_t arm_permitted,
    uint32_t timestamp_ms);
FailsafeState FailsafeStateMachine_GetState(
    const FailsafeStateMachine *machine);
uint8_t FailsafeStateMachine_MotorsMayRun(
    const FailsafeStateMachine *machine);

#endif
