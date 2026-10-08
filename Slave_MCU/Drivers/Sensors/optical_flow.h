#ifndef SLAVE_MCU_DRIVERS_SENSORS_OPTICAL_FLOW_H
#define SLAVE_MCU_DRIVERS_SENSORS_OPTICAL_FLOW_H

#include "stm32f10x.h"

typedef struct {
    uint16_t distance;           // 测距距离（毫米）
    uint8_t  signal_strength;    // 模块原始信号强度
    int32_t  flow_x;             // X轴光流（32位有符号，原始值）
    int32_t  flow_y;             // Y轴光流（32位有符号，原始值）
    uint8_t  data_valid;         // 数据有效标志（1=有效）
    uint8_t  firmware_version;   // 固件版本号
    uint8_t  checksum_valid;     // 当前未确认校验规则，保持 0
} optical_flow_data_t;

extern optical_flow_data_t optical_flow_data;
extern volatile uint8_t optical_flow_rx_ready;

void     optical_flow_init(void);
void     optical_flow_process(uint32_t now_ms);
void     optical_flow_timeout_check(uint32_t now_ms);
void     optical_flow_pause(void);
void     optical_flow_resume(void);
typedef struct {
    uint32_t flow_frames;
    uint32_t distance_frames;
    uint32_t frame_errors;
    uint32_t hardware_errors;
    uint32_t buffer_overruns;
    uint32_t flash_pauses;
} optical_flow_stats_t;
void optical_flow_get_stats(optical_flow_stats_t *stats);
/* Main loop owns parsing and snapshots; ISR only publishes receive events. */
void     optical_flow_get_snapshot(optical_flow_data_t *snapshot);
uint8_t  optical_flow_has_new_data(void);
uint8_t  optical_flow_is_valid(void);
int32_t  optical_flow_get_x(void);
int32_t  optical_flow_get_y(void);
uint16_t optical_flow_get_distance(void);
uint8_t  optical_flow_get_signal_strength(void);

#endif
