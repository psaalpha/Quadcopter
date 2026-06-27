#ifndef __QMC5883P_H
#define __QMC5883P_H

#include "stdint.h"

/* QMC5883P 地址。 */
#define QMC5883P_ADDR        0x2C
#define QMC5883P_ADDR_WRITE  (QMC5883P_ADDR << 1)
#define QMC5883P_ADDR_READ   (QMC5883P_ADDR << 1 | 0x01)

/* 寄存器地址。 */
#define QMC5883P_REG_CHIP_ID 0x00
#define QMC5883P_REG_X_LSB   0x01
#define QMC5883P_REG_X_MSB   0x02
#define QMC5883P_REG_Y_LSB   0x03
#define QMC5883P_REG_Y_MSB   0x04
#define QMC5883P_REG_Z_LSB   0x05
#define QMC5883P_REG_Z_MSB   0x06
#define QMC5883P_REG_STATUS  0x09
#define QMC5883P_REG_CFG1    0x0A
#define QMC5883P_REG_CFG2    0x0B

/* 状态寄存器位定义。 */
#define QMC5883P_STAT_DRDY   (1 << 0)
#define QMC5883P_STAT_OVFL   (1 << 1)

/* 控制寄存器 1 位定义。 */
#define QMC5883P_CFG1_OSR2_1     0x00
#define QMC5883P_CFG1_OSR1_8     0x00
#define QMC5883P_CFG1_ODR_10HZ   0x00
#define QMC5883P_CFG1_ODR_50HZ   0x04
#define QMC5883P_CFG1_ODR_100HZ  0x08
#define QMC5883P_CFG1_ODR_200HZ  0x0C
#define QMC5883P_CFG1_MODE_STANDBY 0x00
#define QMC5883P_CFG1_MODE_NORMAL  0x01

/* 控制寄存器 2 位定义。 */
#define QMC5883P_CFG2_SOFT_RST   (1 << 7)
#define QMC5883P_CFG2_RNG_2G     0x30
#define QMC5883P_CFG2_SET_RESET  0x00

/* 全局测量与校准数据。 */
extern float QMC5883P_Yaw;
extern int16_t qmc_x_raw;
extern int16_t qmc_y_raw;
extern int16_t qmc_z_raw;
extern float QMC5883P_Yaw_Last;
extern int16_t qmc_x_max, qmc_x_min, qmc_y_max, qmc_y_min;

/* 对外接口。 */
uint8_t QMC5883P_Init(void);
void QMC5883P_UpdateYaw(void);
void QMC5883P_Calibrate_Start(void);
void QMC5883P_Calibrate_Collect(void);
void QMC5883P_Calibrate_End(void);

/* 内部批量读写接口声明。 */
static void QMC5883P_I2C_WriteRegs(uint8_t reg, uint8_t *data, uint8_t len);
static void QMC5883P_I2C_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len);

#endif
