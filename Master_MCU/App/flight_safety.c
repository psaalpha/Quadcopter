#include "flight_safety.h"

void FlightSafety_Init(FlightSafetyContext *context)
{
    if (context == 0)
    {
        return;
    }

    context->state = FLIGHT_SAFETY_STARTUP_LOCK;
    context->last_valid_rc_tick = 0u;
    context->failsafe_count = 0u;
    context->link_ok = 0u;
}

FlightSafetyState FlightSafety_OnValidRcFrame(
    FlightSafetyContext *context,
    uint32_t now_tick,
    uint8_t throttle_percent,
    uint8_t low_throttle_threshold)
{
    if (context == 0)
    {
        return FLIGHT_SAFETY_STARTUP_LOCK;
    }

    context->last_valid_rc_tick = now_tick;
    context->link_ok = 1u;

    if (context->state == FLIGHT_SAFETY_LINK_LOSS)
    {
        context->state = FLIGHT_SAFETY_RECOVERY_LOCK;
    }

    if ((context->state != FLIGHT_SAFETY_ACTIVE) &&
        (throttle_percent <= low_throttle_threshold))
    {
        context->state = FLIGHT_SAFETY_ACTIVE;
    }

    return context->state;
}

uint8_t FlightSafety_CheckTimeout(
    FlightSafetyContext *context,
    uint32_t now_tick,
    uint32_t timeout_ticks)
{
    if ((context == 0) || (context->link_ok == 0u))
    {
        return 0u;
    }

    if ((uint32_t)(now_tick - context->last_valid_rc_tick) < timeout_ticks)
    {
        return 0u;
    }

    context->link_ok = 0u;
    context->state = FLIGHT_SAFETY_LINK_LOSS;
    context->failsafe_count++;
    return 1u;
}

uint8_t FlightSafety_MotorsAllowed(const FlightSafetyContext *context)
{
    if (context == 0)
    {
        return 0u;
    }
    return (context->state == FLIGHT_SAFETY_ACTIVE) ? 1u : 0u;
}

uint8_t FlightSafety_LinkOk(const FlightSafetyContext *context)
{
    if (context == 0)
    {
        return 0u;
    }
    return context->link_ok;
}
