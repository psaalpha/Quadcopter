#ifndef __SLAVE_MCU_H
#define __SLAVE_MCU_H

#include "stm32f10x.h"

/* ============================================
 * 从机传感器数据结构体
 * 主循环直接读取，无锁（ISR 整体更新）
 * ============================================ */
typedef struct {
    float    flow_altitude;   // 光流测距高度 (cm)，从 mm 转换
    float    baro_altitude;   // 气压计相对高度 (cm)
    int32_t  flow_x;          // 光流 X 轴原始值
    int32_t  flow_y;          // 光流 Y 轴原始值
    float    mag_yaw;         // 磁力计航向角 (0~360°)
    uint16_t flow_distance;   // 光流测距原始值 (mm)
    uint8_t  flow_quality;    // 光流信号强度 (0~100)
    uint16_t status_flags;    // 从控传感器有效性和告警标志
    uint16_t sequence;        // 最近接收的数据帧序号
    uint32_t timestamp_ms;    // 从控采样时间戳
    int32_t  pressure_pa;     // 气压 (Pa)
    int16_t  temperature_centi_c; // 温度 (0.01°C)
    uint16_t battery_mv;      // 从控测得的电池电压 (mV)
    volatile uint32_t frames_received;
    volatile uint32_t crc_errors;
    volatile uint32_t format_errors;
    volatile uint32_t sequence_gaps;
    volatile uint8_t updated; // 新数据标记：ISR 置 1，主循环读后清 0
} SlaveSensor_t;

/* ============================================
 * 全局实例（main.c 中 extern 引用）
 * ============================================ */
extern SlaveSensor_t slave;

/* ============================================
 * Public API
 * ============================================ */
void SlaveMCU_Init(void);

#endif /* __SLAVE_MCU_H */
