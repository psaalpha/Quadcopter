#include "flight_safety.h"

#include <assert.h>
#include <stdio.h>

static void AssertStartupLock(void)
{
    FlightSafetyContext context;

    FlightSafety_Init(&context);
    assert(context.state == FLIGHT_SAFETY_STARTUP_LOCK);
    assert(FlightSafety_LinkOk(&context) == 0u);
    assert(FlightSafety_MotorsAllowed(&context) == 0u);

    FlightSafety_OnValidRcFrame(&context, 10u, 75u, 5u);
    assert(context.state == FLIGHT_SAFETY_STARTUP_LOCK);
    assert(FlightSafety_LinkOk(&context) == 1u);
    assert(FlightSafety_MotorsAllowed(&context) == 0u);

    FlightSafety_OnValidRcFrame(&context, 11u, 5u, 5u);
    assert(context.state == FLIGHT_SAFETY_ACTIVE);
    assert(FlightSafety_MotorsAllowed(&context) == 1u);
}

static void AssertFailsafeAndRecoveryLock(void)
{
    FlightSafetyContext context;

    FlightSafety_Init(&context);
    FlightSafety_OnValidRcFrame(&context, 100u, 0u, 5u);
    assert(FlightSafety_CheckTimeout(&context, 159u, 60u) == 0u);
    assert(FlightSafety_CheckTimeout(&context, 160u, 60u) == 1u);
    assert(context.state == FLIGHT_SAFETY_LINK_LOSS);
    assert(context.failsafe_count == 1u);
    assert(FlightSafety_MotorsAllowed(&context) == 0u);

    FlightSafety_OnValidRcFrame(&context, 161u, 50u, 5u);
    assert(context.state == FLIGHT_SAFETY_RECOVERY_LOCK);
    assert(FlightSafety_LinkOk(&context) == 1u);
    assert(FlightSafety_MotorsAllowed(&context) == 0u);

    FlightSafety_OnValidRcFrame(&context, 162u, 4u, 5u);
    assert(context.state == FLIGHT_SAFETY_ACTIVE);
    assert(FlightSafety_MotorsAllowed(&context) == 1u);
}

static void AssertTickWraparound(void)
{
    FlightSafetyContext context;

    FlightSafety_Init(&context);
    FlightSafety_OnValidRcFrame(&context, 0xFFFFFFF0u, 0u, 5u);
    assert(FlightSafety_CheckTimeout(&context, 0x00000005u, 32u) == 0u);
    assert(FlightSafety_CheckTimeout(&context, 0x00000010u, 32u) == 1u);
}

int main(void)
{
    AssertStartupLock();
    AssertFailsafeAndRecoveryLock();
    AssertTickWraparound();

    puts("flight_safety_test: PASS");
    return 0;
}
