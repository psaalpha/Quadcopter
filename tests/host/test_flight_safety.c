#include "flight_safety.h"

#include <assert.h>
#include <stdio.h>

static void AssertStartupLock(void)
{
    flight_safety_context_t context;

    flight_safety_init(&context);
    assert(context.state == FLIGHT_SAFETY_STARTUP_LOCK);
    assert(flight_safety_link_ok(&context) == 0u);
    assert(flight_safety_motors_allowed(&context) == 0u);

    flight_safety_on_valid_rc_frame(&context, 10u, 75u, 5u);
    assert(context.state == FLIGHT_SAFETY_STARTUP_LOCK);
    assert(flight_safety_link_ok(&context) == 1u);
    assert(flight_safety_motors_allowed(&context) == 0u);

    flight_safety_on_valid_rc_frame(&context, 11u, 5u, 5u);
    assert(context.state == FLIGHT_SAFETY_ACTIVE);
    assert(flight_safety_motors_allowed(&context) == 1u);
}

static void AssertFailsafeAndRecoveryLock(void)
{
    flight_safety_context_t context;

    flight_safety_init(&context);
    flight_safety_on_valid_rc_frame(&context, 100u, 0u, 5u);
    assert(flight_safety_check_timeout(&context, 159u, 60u) == 0u);
    assert(flight_safety_check_timeout(&context, 160u, 60u) == 1u);
    assert(context.state == FLIGHT_SAFETY_LINK_LOSS);
    assert(context.failsafe_count == 1u);
    assert(flight_safety_motors_allowed(&context) == 0u);

    flight_safety_on_valid_rc_frame(&context, 161u, 50u, 5u);
    assert(context.state == FLIGHT_SAFETY_RECOVERY_LOCK);
    assert(flight_safety_link_ok(&context) == 1u);
    assert(flight_safety_motors_allowed(&context) == 0u);

    flight_safety_on_valid_rc_frame(&context, 162u, 4u, 5u);
    assert(context.state == FLIGHT_SAFETY_ACTIVE);
    assert(flight_safety_motors_allowed(&context) == 1u);
}

static void AssertTickWraparound(void)
{
    flight_safety_context_t context;

    flight_safety_init(&context);
    flight_safety_on_valid_rc_frame(&context, 0xFFFFFFF0u, 0u, 5u);
    assert(flight_safety_check_timeout(&context, 0x00000005u, 32u) == 0u);
    assert(flight_safety_check_timeout(&context, 0x00000010u, 32u) == 1u);
}

int main(void)
{
    AssertStartupLock();
    AssertFailsafeAndRecoveryLock();
    AssertTickWraparound();

    puts("flight_safety_test: PASS");
    return 0;
}
