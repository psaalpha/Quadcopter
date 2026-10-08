#ifndef MASTER_MCU_APP_FLIGHT_SAFETY_H
#define MASTER_MCU_APP_FLIGHT_SAFETY_H

#include <stdint.h>

typedef enum
{
    FLIGHT_SAFETY_STARTUP_LOCK = 0,
    FLIGHT_SAFETY_ACTIVE,
    FLIGHT_SAFETY_LINK_LOSS,
    FLIGHT_SAFETY_RECOVERY_LOCK
} flight_safety_state_t;

typedef struct
{
    flight_safety_state_t state;
    uint32_t last_valid_rc_tick;
    uint32_t failsafe_count;
    uint8_t link_ok;
} flight_safety_context_t;

void flight_safety_init(flight_safety_context_t *context);

flight_safety_state_t flight_safety_on_valid_rc_frame(
    flight_safety_context_t *context,
    uint32_t now_tick,
    uint8_t throttle_percent,
    uint8_t low_throttle_threshold);

uint8_t flight_safety_check_timeout(
    flight_safety_context_t *context,
    uint32_t now_tick,
    uint32_t timeout_ticks);

uint8_t flight_safety_motors_allowed(const flight_safety_context_t *context);
uint8_t flight_safety_link_ok(const flight_safety_context_t *context);

#endif
