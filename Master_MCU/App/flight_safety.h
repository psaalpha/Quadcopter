#ifndef FLIGHT_SAFETY_H
#define FLIGHT_SAFETY_H

#include <stdint.h>

typedef enum
{
    FLIGHT_SAFETY_STARTUP_LOCK = 0,
    FLIGHT_SAFETY_ACTIVE,
    FLIGHT_SAFETY_LINK_LOSS,
    FLIGHT_SAFETY_RECOVERY_LOCK
} FlightSafetyState;

typedef struct
{
    FlightSafetyState state;
    uint32_t last_valid_rc_tick;
    uint32_t failsafe_count;
    uint8_t link_ok;
} FlightSafetyContext;

void FlightSafety_Init(FlightSafetyContext *context);

FlightSafetyState FlightSafety_OnValidRcFrame(
    FlightSafetyContext *context,
    uint32_t now_tick,
    uint8_t throttle_percent,
    uint8_t low_throttle_threshold);

uint8_t FlightSafety_CheckTimeout(
    FlightSafetyContext *context,
    uint32_t now_tick,
    uint32_t timeout_ticks);

uint8_t FlightSafety_MotorsAllowed(const FlightSafetyContext *context);
uint8_t FlightSafety_LinkOk(const FlightSafetyContext *context);

#endif
