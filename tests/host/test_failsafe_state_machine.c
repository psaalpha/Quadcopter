#include "failsafe_state_machine.h"

#include <assert.h>
#include <stdio.h>

static void TestGuardedArmAndWarningRecovery(void)
{
    FailsafeStateMachine machine;

    FailsafeStateMachine_Init(&machine, 0u);
    assert(FailsafeStateMachine_GetState(&machine) == FAILSAFE_STATE_DISARM);
    assert(FailsafeStateMachine_MotorsMayRun(&machine) == 0u);

    assert(FailsafeStateMachine_Handle(
        &machine,
        FAILSAFE_EVENT_ARM_REQUEST,
        0u,
        10u) == FAILSAFE_RESULT_GUARD_REJECTED);
    assert(FailsafeStateMachine_GetState(&machine) == FAILSAFE_STATE_DISARM);

    assert(FailsafeStateMachine_Handle(
        &machine,
        FAILSAFE_EVENT_ARM_REQUEST,
        1u,
        20u) == FAILSAFE_RESULT_TRANSITIONED);
    assert(FailsafeStateMachine_GetState(&machine) == FAILSAFE_STATE_NORMAL);
    assert(FailsafeStateMachine_MotorsMayRun(&machine) == 1u);

    assert(FailsafeStateMachine_Handle(
        &machine,
        FAILSAFE_EVENT_WARNING_PRESENT,
        1u,
        30u) == FAILSAFE_RESULT_TRANSITIONED);
    assert(FailsafeStateMachine_GetState(&machine) == FAILSAFE_STATE_WARNING);
    assert(FailsafeStateMachine_MotorsMayRun(&machine) == 1u);

    assert(FailsafeStateMachine_Handle(
        &machine,
        FAILSAFE_EVENT_WARNINGS_CLEARED,
        1u,
        40u) == FAILSAFE_RESULT_TRANSITIONED);
    assert(FailsafeStateMachine_GetState(&machine) == FAILSAFE_STATE_NORMAL);
}

static void TestCriticalFaultCannotAutoRearm(void)
{
    FailsafeStateMachine machine;

    FailsafeStateMachine_Init(&machine, 0u);
    assert(FailsafeStateMachine_Handle(
        &machine,
        FAILSAFE_EVENT_ARM_REQUEST,
        1u,
        1u) == FAILSAFE_RESULT_TRANSITIONED);
    assert(FailsafeStateMachine_Handle(
        &machine,
        FAILSAFE_EVENT_CRITICAL_FAULT,
        1u,
        2u) == FAILSAFE_RESULT_TRANSITIONED);
    assert(FailsafeStateMachine_GetState(&machine) == FAILSAFE_STATE_FAILSAFE);
    assert(FailsafeStateMachine_MotorsMayRun(&machine) == 0u);

    assert(FailsafeStateMachine_Handle(
        &machine,
        FAILSAFE_EVENT_WARNINGS_CLEARED,
        1u,
        3u) == FAILSAFE_RESULT_NO_CHANGE);
    assert(FailsafeStateMachine_Handle(
        &machine,
        FAILSAFE_EVENT_ACTION_COMPLETE,
        1u,
        4u) == FAILSAFE_RESULT_TRANSITIONED);
    assert(FailsafeStateMachine_GetState(&machine) == FAILSAFE_STATE_DISARM);
    assert(machine.transition_count == 3u);
}

static void TestDisarmDominatesEveryActiveState(void)
{
    FailsafeStateMachine machine;

    FailsafeStateMachine_Init(&machine, 0u);
    assert(FailsafeStateMachine_Handle(
        &machine,
        FAILSAFE_EVENT_ARM_REQUEST,
        1u,
        1u) == FAILSAFE_RESULT_TRANSITIONED);
    assert(FailsafeStateMachine_Handle(
        &machine,
        FAILSAFE_EVENT_DISARM_REQUEST,
        1u,
        2u) == FAILSAFE_RESULT_TRANSITIONED);
    assert(FailsafeStateMachine_GetState(&machine) == FAILSAFE_STATE_DISARM);
}

int main(void)
{
    TestGuardedArmAndWarningRecovery();
    TestCriticalFaultCannotAutoRearm();
    TestDisarmDominatesEveryActiveState();

    puts("failsafe state machine tests passed");
    return 0;
}
