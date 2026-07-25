#ifndef __LED_H
#define __LED_H

#include "driver_status.h"

typedef enum
{
    BOARD_LED_STATUS = 0,
    BOARD_LED_AUXILIARY_1,
    BOARD_LED_AUXILIARY_2,
    BOARD_LED_COUNT
} BoardLedId;

typedef enum
{
    BOARD_LED_OFF = 0,
    BOARD_LED_ON = 1
} BoardLedState;

DriverStatus BoardLed_Init(void);
DriverStatus BoardLed_Set(BoardLedId led, BoardLedState state);
const DriverHealth *BoardLed_GetHealth(BoardLedId led);

/* Compatibility API retained while application call sites migrate. */
void LED_Init(void);
void LED1_ON(void);
void LED1_OFF(void);

void LED2_ON(void);
void LED2_OFF(void);

void LED3_ON(void);
void LED3_OFF(void);

#endif
