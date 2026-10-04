#ifndef __PWM4_H
#define __PWM4_H

#include <stdint.h>


void PWM4_Init(void);
void PWM4_SetMinimumOutput(void);
void PWM4_SetCompare1(uint16_t Compare);
void PWM4_SetCompare2(uint16_t Compare);
void PWM4_SetCompare3(uint16_t Compare);
void PWM4_SetCompare4(uint16_t Compare);
#endif


