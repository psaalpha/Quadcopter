#ifndef __OPTICALFLOW_H
#define __OPTICALFLOW_H

#include "stm32f10x.h"

typedef struct {
    uint16_t distance;           // 测距距离（毫米）
    uint8_t  signal_strength;    // 信号强度（0-100）
    int32_t  flow_x;             // X轴光流（32位有符号，原始值）
    int32_t  flow_y;             // Y轴光流（32位有符号，原始值）
    uint8_t  data_valid;         // 数据有效标志（1=有效）
    uint8_t  firmware_version;   // 固件版本号
    uint8_t  checksum_valid;     // 校验通过标志
} OpticalFlow_Data_t;

extern OpticalFlow_Data_t OpticalFlow_Data;
extern volatile uint8_t OpticalFlow_RxFlag;

void     OpticalFlow_Init(void);
void     OpticalFlow_TimeoutCheck(void);
uint8_t  OpticalFlow_HasNewData(void);
uint8_t  IsDataValid(void);
int32_t  GetFlowX(void);
int32_t  GetFlowY(void);
uint16_t GetDistance(void);
uint8_t  GetSignalStrength(void);

#endif
