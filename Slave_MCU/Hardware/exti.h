#ifndef SLAVE_EXTI_H
#define SLAVE_EXTI_H
#include "stm32f10x.h"
typedef struct {
    uint8_t key;
    uint8_t servo_changed;
    uint8_t calibration_pulse;
} SlaveInputEvents;
void EXTI_Inputs_Init(void);
/* Main loop atomically takes notifications before handling them. */
void EXTI_Inputs_Take(SlaveInputEvents *events);
uint8_t EXTI_ServoLevel(void);
uint8_t EXTI_KeyLevel(void);
#endif
