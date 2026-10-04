#ifndef __OPTICALFLOW_H
#define __OPTICALFLOW_H

#include "stm32f10x.h"

typedef struct {
    uint16_t distance;           // 测距距离（毫米）
    uint8_t  signal_strength;    // 模块原始信号强度
    int32_t  flow_x;             // X轴光流（32位有符号，原始值）
    int32_t  flow_y;             // Y轴光流（32位有符号，原始值）
    uint8_t  data_valid;         // 数据有效标志（1=有效）
    uint8_t  firmware_version;   // 固件版本号
    uint8_t  checksum_valid;     // 当前未确认校验规则，保持 0
} OpticalFlow_Data_t;

extern OpticalFlow_Data_t OpticalFlow_Data;
extern volatile uint8_t OpticalFlow_RxFlag;

void     OpticalFlow_Init(void);
void     OpticalFlow_Process(uint32_t now_ms);
void     OpticalFlow_TimeoutCheck(uint32_t now_ms);
void     OpticalFlow_Pause(void);
void     OpticalFlow_Resume(void);
typedef struct {
    uint32_t flow_frames;
    uint32_t distance_frames;
    uint32_t frame_errors;
    uint32_t hardware_errors;
    uint32_t buffer_overruns;
    uint32_t flash_pauses;
} OpticalFlow_Stats;
void OpticalFlow_GetStats(OpticalFlow_Stats *stats);
/* Main loop owns parsing and snapshots; ISR only publishes receive events. */
void     OpticalFlow_GetSnapshot(OpticalFlow_Data_t *snapshot);
uint8_t  OpticalFlow_HasNewData(void);
uint8_t  IsDataValid(void);
int32_t  GetFlowX(void);
int32_t  GetFlowY(void);
uint16_t GetDistance(void);
uint8_t  GetSignalStrength(void);

#endif
