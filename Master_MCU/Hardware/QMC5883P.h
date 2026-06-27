#ifndef __QMC5883P_H
#define __QMC5883P_H

#include "stdint.h"

// ========== QMC5883P 核心配置（严格按手册5.4/9节）==========
#define QMC5883P_ADDR        0x2C        // 7位基础I2C地址（手册5.4节强制规定）
#define QMC5883P_ADDR_WRITE  (QMC5883P_ADDR << 1)       // 写地址：0x58
#define QMC5883P_ADDR_READ   (QMC5883P_ADDR << 1 | 0x01) // 读地址：0x59

// 寄存器地址（手册9.1节 Register Map 严格定义）
#define QMC5883P_REG_CHIP_ID 0x00        // 芯片ID寄存器（只读，默认0x80）
#define QMC5883P_REG_X_LSB   0x01        // X轴数据低字节
#define QMC5883P_REG_X_MSB   0x02        // X轴数据高字节
#define QMC5883P_REG_Y_LSB   0x03        // Y轴数据低字节
#define QMC5883P_REG_Y_MSB   0x04        // Y轴数据高字节
#define QMC5883P_REG_Z_LSB   0x05        // Z轴数据低字节
#define QMC5883P_REG_Z_MSB   0x06        // Z轴数据高字节
#define QMC5883P_REG_STATUS  0x09        // 状态寄存器（09H：DRDY/OVFL）
#define QMC5883P_REG_CFG1    0x0A        // 控制寄存器1（模式/ODR/OSR）
#define QMC5883P_REG_CFG2    0x0B        // 控制寄存器2（软重置/量程/自检）

// 状态寄存器 位定义（手册9.2.2节）
#define QMC5883P_STAT_DRDY   (1 << 0)    // BIT0：数据就绪（1=新数据就绪）
#define QMC5883P_STAT_OVFL   (1 << 1)    // BIT1：数据溢出（1=超出±30000LSB）

// 控制寄存器1 (0x0A) 位定义（手册9.2.3节 Table17）
#define QMC5883P_CFG1_OSR2_1     0x00    // OSR2=1 (默认)
#define QMC5883P_CFG1_OSR1_8     0x00    // OSR1=8 (推荐，低噪声)
#define QMC5883P_CFG1_ODR_10HZ   0x00    // 输出速率10Hz
#define QMC5883P_CFG1_ODR_50HZ   0x04    // 输出速率50Hz
#define QMC5883P_CFG1_ODR_100HZ  0x08    // 输出速率100Hz
#define QMC5883P_CFG1_ODR_200HZ  0x0C    // 输出速率200Hz（新增：高速模式）
#define QMC5883P_CFG1_MODE_STANDBY 0x00  // 待机模式（默认）
#define QMC5883P_CFG1_MODE_NORMAL  0x01  // 正常连续测量模式（推荐）

// 控制寄存器2 (0x0B) 位定义（手册9.2.3节 Table18）
#define QMC5883P_CFG2_SOFT_RST   (1 << 7)    // BIT7：软重置
#define QMC5883P_CFG2_RNG_2G     0x30        // BIT5-4=11：±2G（最高灵敏度）
#define QMC5883P_CFG2_SET_RESET  0x00        // 自动SET/RESET（默认，必开）

// ========== 全局变量声明 ==========
extern float QMC5883P_Yaw;          // 航向角（0-360度）
extern int16_t qmc_x_raw;           // X轴原始数据（校准后）
extern int16_t qmc_y_raw;           // Y轴原始数据（校准后）
extern int16_t qmc_z_raw;           // Z轴原始数据（校准后）
extern float QMC5883P_Yaw_Last;     // 上一次航向角
extern int16_t qmc_x_max, qmc_x_min, qmc_y_max, qmc_y_min; // 校准极值

// ========== 函数声明 ==========
uint8_t QMC5883P_Init(void);        // 初始化QMC5883P
void QMC5883P_UpdateYaw(void);      // 更新航向角
void QMC5883P_Calibrate_Start(void);// 校准开始（重置极值）
void QMC5883P_Calibrate_Collect(void);// 校准数据收集（循环调用）
void QMC5883P_Calibrate_End(void);  // 校准结束（计算偏移）

// 新增批量读写函数声明（内部使用，可选）
static void QMC5883P_I2C_WriteRegs(uint8_t reg, uint8_t *data, uint8_t len);
static void QMC5883P_I2C_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len);

#endif
