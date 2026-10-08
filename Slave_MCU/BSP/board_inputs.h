#ifndef SLAVE_MCU_BSP_BOARD_INPUTS_H
#define SLAVE_MCU_BSP_BOARD_INPUTS_H
#include "stm32f10x.h"
typedef struct {
    uint8_t key;
    uint8_t servo_changed;
    uint8_t calibration_pulse;
} board_input_events_t;
void board_inputs_init(void);
/* Main loop atomically takes notifications before handling them. */
void board_inputs_take(board_input_events_t *events);
uint8_t board_inputs_servo_level(void);
uint8_t board_inputs_key_level(void);
#endif
