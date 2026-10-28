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
    context->arm_low_seen = 0u;
}

flight_safety_state_t flight_safety_on_valid_rc_frame(
    flight_safety_context_t *context,
    uint32_t now_tick,
    uint8_t throttle_percent,
    uint8_t low_throttle_threshold,
    uint8_t arm_switch, uint8_t preflight_ok)
{
    if (context == 0)
    {
        return FLIGHT_SAFETY_STARTUP_LOCK;
    }

    context->last_valid_rc_tick = now_tick;
    context->link_ok = 1u;

    if (arm_switch == 0u) {
        context->state = FLIGHT_SAFETY_DISARMED;
        context->arm_low_seen = 1u;
    } else if (arm_switch == 1u && context->state != FLIGHT_SAFETY_ACTIVE) {
        if (context->arm_low_seen != 0u) {
            context->arm_low_seen = 0u;
            context->state = (preflight_ok != 0u && throttle_percent <= low_throttle_threshold) ?
                FLIGHT_SAFETY_ACTIVE : FLIGHT_SAFETY_RECOVERY_LOCK;
        } else if (context->state == FLIGHT_SAFETY_LINK_LOSS) {
            context->state = FLIGHT_SAFETY_RECOVERY_LOCK;
        }
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
    context->arm_low_seen = 0u;
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

void flight_safety_latch_fault(flight_safety_context_t *context)
{
    if(context){context->state=FLIGHT_SAFETY_SENSOR_FAULT;context->arm_low_seen=0u;}
}
void flight_safety_disarm(flight_safety_context_t *context)
{
    if(context){context->state=FLIGHT_SAFETY_DISARMED;context->arm_low_seen=0u;}
}
