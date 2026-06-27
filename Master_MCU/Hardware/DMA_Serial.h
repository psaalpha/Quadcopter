#ifndef __DMA_Serial_H
#define __DMA_Serial_H


void BlueSerial_Init(void);
void BlueSerial_DMA_Send(uint8_t *buff, uint16_t len);
void BlueSerial_SendByte(uint8_t Byte);
void BlueSerial_SendArray(uint8_t *Array, uint16_t Length);
void BlueSerial_SendString(char *String);
uint32_t BlueSerial_Pow(uint32_t X, uint32_t Y);
void BlueSerial_SendNumber(uint32_t Number, uint8_t Length);
void BlueSerial_Printf(char *format, ...);
void BlueSerial_SendBuff(uint8_t *Buff, uint16_t Len);
void PID_Param_Parse(void);
float Pitch_Back_Kp(void);
float Pitch_Back_Ki(void);
float Pitch_Back_Kd(void); 
float Back_Mid(void) ;     
float Roll_Back_Kp(void) ; 
float Roll_Back_Ki(void) ; 
float Roll_Back_Kd(void) ; 
float Yaw_Back_Kp(void);   
float Yaw_Back_Ki(void);   
float Yaw_Back_Kd(void);   
float Roll_Back_Aim(void) ;
float Pitch_Back_Aim(void);
uint8_t Back_Base_Duty(void);
float Pitch_Angle_Back_Kp(void);
float Roll_Angle_Back_Kp(void); 
float Yaw_Angle_Back_Kp(void);  

#endif
