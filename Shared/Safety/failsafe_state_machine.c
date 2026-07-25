#include "failsafe_state_machine.h"

#include <limits.h>

static FailsafeResult FailsafeStateMachine_Transition(
    FailsafeStateMachine *machine,
    FailsafeState state,
    FailsafeEvent event,
    uint32_t timestamp_ms)
{
    if (machine->state == state)
    {
        machine->last_event = event;
        return FAILSAFE_RESULT_NO_CHANGE;
    }

    machine->previous_state = machine->state;
    machine->state = state;
    machine->last_event = event;
    machine->state_entered_ms = timestamp_ms;
    if (machine->transition_count != UINT32_MAX)
    {
        machine->transition_count++;
    }
    return FAILSAFE_RESULT_TRANSITIONED;
}

void FailsafeStateMachine_Init(
    FailsafeStateMachine *machine,
    uint32_t timestamp_ms)
{
    if (machine == 0)
    {
        return;
    }

    machine->state = FAILSAFE_STATE_DISARM;
    machine->previous_state = FAILSAFE_STATE_DISARM;
    machine->last_event = FAILSAFE_EVENT_RESET;
    machine->state_entered_ms = timestamp_ms;
    machine->transition_count = 0u;
    machine->initialized = 1u;
}

FailsafeResult FailsafeStateMachine_Handle(
    FailsafeStateMachine *machine,
    FailsafeEvent event,
    uint8_t arm_permitted,
    uint32_t timestamp_ms)
{
    if ((machine == 0) || !machine->initialized
        || (event > FAILSAFE_EVENT_RESET))
    {
        return FAILSAFE_RESULT_INVALID_ARGUMENT;
    }

    if (event == FAILSAFE_EVENT_RESET)
    {
        return FailsafeStateMachine_Transition(
            machine,
            FAILSAFE_STATE_DISARM,
            event,
            timestamp_ms);
    }
    if (event == FAILSAFE_EVENT_DISARM_REQUEST)
    {
        return FailsafeStateMachine_Transition(
            machine,
            FAILSAFE_STATE_DISARM,
            event,
            timestamp_ms);
    }
    if (event == FAILSAFE_EVENT_CRITICAL_FAULT)
    {
        if (machine->state == FAILSAFE_STATE_DISARM)
        {
            return FAILSAFE_RESULT_NO_CHANGE;
        }
        return FailsafeStateMachine_Transition(
            machine,
            FAILSAFE_STATE_FAILSAFE,
            event,
            timestamp_ms);
    }

    switch (machine->state)
    {
        case FAILSAFE_STATE_DISARM:
            if (event == FAILSAFE_EVENT_ARM_REQUEST)
            {
                if (!arm_permitted)
                {
                    machine->last_event = event;
                    return FAILSAFE_RESULT_GUARD_REJECTED;
                }
                return FailsafeStateMachine_Transition(
                    machine,
                    FAILSAFE_STATE_NORMAL,
                    event,
                    timestamp_ms);
            }
            break;

        case FAILSAFE_STATE_NORMAL:
            if (event == FAILSAFE_EVENT_WARNING_PRESENT)
            {
                return FailsafeStateMachine_Transition(
                    machine,
                    FAILSAFE_STATE_WARNING,
                    event,
                    timestamp_ms);
            }
            break;

        case FAILSAFE_STATE_WARNING:
            if (event == FAILSAFE_EVENT_WARNINGS_CLEARED)
            {
                return FailsafeStateMachine_Transition(
                    machine,
                    FAILSAFE_STATE_NORMAL,
                    event,
                    timestamp_ms);
            }
            break;

        case FAILSAFE_STATE_FAILSAFE:
            if (event == FAILSAFE_EVENT_ACTION_COMPLETE)
            {
                return FailsafeStateMachine_Transition(
                    machine,
                    FAILSAFE_STATE_DISARM,
                    event,
                    timestamp_ms);
            }
            break;

        default:
            return FAILSAFE_RESULT_INVALID_ARGUMENT;
    }

    machine->last_event = event;
    return FAILSAFE_RESULT_NO_CHANGE;
}

FailsafeState FailsafeStateMachine_GetState(
    const FailsafeStateMachine *machine)
{
    return ((machine != 0) && machine->initialized)
        ? machine->state
        : FAILSAFE_STATE_DISARM;
}

uint8_t FailsafeStateMachine_MotorsMayRun(
    const FailsafeStateMachine *machine)
{
    if ((machine == 0) || !machine->initialized)
    {
        return 0u;
    }

    return ((machine->state == FAILSAFE_STATE_NORMAL)
        || (machine->state == FAILSAFE_STATE_WARNING))
        ? 1u
        : 0u;
}
