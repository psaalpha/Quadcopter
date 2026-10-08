#include "flight_safety.h"

void flight_safety_init(flight_safety_context_t *context)
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

flight_safety_state_t flight_safety_on_valid_rc_frame(
    flight_safety_context_t *context,
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

uint8_t flight_safety_check_timeout(
    flight_safety_context_t *context,
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

uint8_t flight_safety_motors_allowed(const flight_safety_context_t *context)
{
    if (context == 0)
    {
        return 0u;
    }
    return (context->state == FLIGHT_SAFETY_ACTIVE) ? 1u : 0u;
}

uint8_t flight_safety_link_ok(const flight_safety_context_t *context)
{
    if (context == 0)
    {
        return 0u;
    }
    return context->link_ok;
}
