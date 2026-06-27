#ifndef __BLUE_SERIAL_H
#define __BLUE_SERIAL_H

#include <stdio.h>

extern char BlueSerial_RxPacket[];
extern uint8_t BlueSerial_RxFlag;

void BlueSerial_Init(void);
void BlueSerial_SendByte(uint8_t Byte);
void BlueSerial_SendArray(uint8_t *Array, uint16_t Length);
void BlueSerial_SendString(char *String);
void BlueSerial_SendNumber(uint32_t Number, uint8_t Length);
void BlueSerial_Printf(char *format, ...);
void BlueSerial_SendBuff(uint8_t *Buff, uint16_t Len);
void PID_Param_Parse(void);
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
