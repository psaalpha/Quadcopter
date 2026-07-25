#ifndef __BLUE_SERIAL_H
#define __BLUE_SERIAL_H

#include <stdio.h>
#include <stdint.h>

extern char BlueSerial_RxPacket[];
extern volatile uint8_t BlueSerial_RxFlag;

/* PID_Param_Parse返回的参数更新位，仅对本次有效帧置位。 */
#define PID_PARAM_UPDATE_PKP            (1UL << 0)
#define PID_PARAM_UPDATE_PKI            (1UL << 1)
#define PID_PARAM_UPDATE_PKD            (1UL << 2)
#define PID_PARAM_UPDATE_MID            (1UL << 3)
#define PID_PARAM_UPDATE_RKP            (1UL << 4)
#define PID_PARAM_UPDATE_RKI            (1UL << 5)
#define PID_PARAM_UPDATE_RKD            (1UL << 6)
#define PID_PARAM_UPDATE_YKP            (1UL << 7)
#define PID_PARAM_UPDATE_YKI            (1UL << 8)
#define PID_PARAM_UPDATE_YKD            (1UL << 9)
#define PID_PARAM_UPDATE_PAKP           (1UL << 10)
#define PID_PARAM_UPDATE_RAKP           (1UL << 11)
#define PID_PARAM_UPDATE_YAKP           (1UL << 12)
#define PID_PARAM_UPDATE_PAIM           (1UL << 13)
#define PID_PARAM_UPDATE_RAIM           (1UL << 14)
#define PID_PARAM_UPDATE_CONTROL_SPEED  (1UL << 15)

void BlueSerial_Init(void);
void BlueSerial_SendByte(uint8_t Byte);
void BlueSerial_SendArray(uint8_t *Array, uint16_t Length);
void BlueSerial_SendString(char *String);
void BlueSerial_SendNumber(uint32_t Number, uint8_t Length);
void BlueSerial_Printf(char *format, ...);
void BlueSerial_SendBuff(uint8_t *Buff, uint16_t Len);
uint32_t PID_Param_Parse(void);
float Pitch_Back_Kp(void) ;
float Pitch_Back_Ki(void) ;
float Pitch_Back_Kd(void) ;
float Back_Mid(void) ;
float Roll_Back_Kp(void) ;
float Roll_Back_Ki(void) ;
float Roll_Back_Kd(void); 
float Yaw_Back_Kp(void) ;
float Yaw_Back_Ki(void) ;
float Yaw_Back_Kd(void) ;
float Pitch_Angle_Back_Kp(void) ;
float Roll_Angle_Back_Kp(void) ;
float Yaw_Angle_Back_Kp(void) ;
float Roll_Back_Aim(void) ;
float Pitch_Back_Aim(void) ;
uint8_t Back_Base_Duty(void) ;

#endif
