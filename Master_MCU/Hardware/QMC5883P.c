#include "QMC5883P.h"
#include <math.h>
#include "stm32f10x.h"
#include "MyI2C.h"   
#include "Delay.h"

/* 磁力计测量结果与校准状态。 */
float QMC5883P_Yaw = 0.0f;
int16_t qmc_x_raw = 0;
int16_t qmc_y_raw = 0;
int16_t qmc_z_raw = 0;
float QMC5883P_Yaw_Last = 0.0f;
int16_t qmc_x_offset = 0;
int16_t qmc_y_offset = 0;
int16_t qmc_x_max = -32768, qmc_x_min = 32767;
int16_t qmc_y_max = -32768, qmc_y_min = 32767;
#define M_PI    3.1415926535
#define FILTER_WINDOW 5  /* 5 点滑动滤波窗口 */
float yaw_filter_buf[FILTER_WINDOW] = {0.0f};
uint8_t filter_idx = 0;

/* 写入单个寄存器。 */
static void QMC5883P_I2C_WriteReg(uint8_t reg, uint8_t data)
{
    MyI2C_Start();
    MyI2C_SendByte(QMC5883P_ADDR_WRITE);
    if(MyI2C_ReceiveAck() != 0) return;
    MyI2C_SendByte(reg);
    if(MyI2C_ReceiveAck() != 0) return;
    MyI2C_SendByte(data);
    if(MyI2C_ReceiveAck() != 0) return;
    MyI2C_Stop();
}

/* 读取单个寄存器。 */
static uint8_t QMC5883P_I2C_ReadReg(uint8_t reg)
{
    uint8_t data = 0;
    MyI2C_Start();
    MyI2C_SendByte(QMC5883P_ADDR_WRITE);
    if(MyI2C_ReceiveAck() != 0) return 0;
    MyI2C_SendByte(reg);
    if(MyI2C_ReceiveAck() != 0) return 0;

    MyI2C_Start();
    MyI2C_SendByte(QMC5883P_ADDR_READ);
    if(MyI2C_ReceiveAck() != 0) return 0;
    data = MyI2C_ReceiveByte();
    MyI2C_SendAck(1); /* NACK */
    MyI2C_Stop();
    return data;
}

/* 连续读取多个寄存器。 */
static void QMC5883P_I2C_ReadRegs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    MyI2C_Start();
    MyI2C_SendByte(QMC5883P_ADDR_WRITE);
    if(MyI2C_ReceiveAck() != 0) return;
    MyI2C_SendByte(reg);
    if(MyI2C_ReceiveAck() != 0) return;

    MyI2C_Start();
    MyI2C_SendByte(QMC5883P_ADDR_READ);
    if(MyI2C_ReceiveAck() != 0) return;

    for(uint8_t i=0; i<len; i++)
    {
        buf[i] = MyI2C_ReceiveByte();
        if(i < len-1) MyI2C_SendAck(0); /* 前 n-1 个字节 ACK */
        else MyI2C_SendAck(1);          /* 最后 1 个字节 NACK */
    }
    MyI2C_Stop();
}

/* 读取 XYZ 原始数据并更新航向角。 */
static void QMC5883P_ReadXYZ(void)
{
    uint8_t buf[6] = {0};
    uint8_t status = 0;

    /* 检查状态寄存器，确保数据就绪且无溢出。 */
    status = QMC5883P_I2C_ReadReg(QMC5883P_REG_STATUS);
    if((status & QMC5883P_STAT_DRDY) == 0) return;
    if((status & QMC5883P_STAT_OVFL) != 0) return;

    /* 连续读取 6 字节 XYZ 数据。 */
    QMC5883P_I2C_ReadRegs(QMC5883P_REG_X_LSB, buf, 6);

    /* 组合为有符号 16 位数据。 */
    int16_t x = (int16_t)(buf[1] << 8) | buf[0];
    int16_t y = (int16_t)(buf[3] << 8) | buf[2];
    int16_t z = (int16_t)(buf[5] << 8) | buf[4];

    /* 应用硬铁校准偏移。 */
    qmc_x_raw = x - qmc_x_offset;
    qmc_y_raw = y - qmc_y_offset;
    qmc_z_raw = z;

    /* 计算航向角，并转换到 0~360 度。 */
    float raw_yaw = atan2((float)qmc_y_raw, (float)qmc_x_raw) * 180.0f / M_PI;
    if(raw_yaw < 0) raw_yaw += 360.0f;

    /* 滑动平均滤波，降低航向角抖动。 */
    yaw_filter_buf[filter_idx++] = raw_yaw;
    if(filter_idx >= FILTER_WINDOW) filter_idx = 0;
    QMC5883P_Yaw = 0.0f;
    for(uint8_t i=0; i<FILTER_WINDOW; i++)
    {
        QMC5883P_Yaw += yaw_filter_buf[i];
    }
    QMC5883P_Yaw /= FILTER_WINDOW;
    QMC5883P_Yaw_Last = QMC5883P_Yaw;
}

/* 初始化 QMC5883P。 */
uint8_t QMC5883P_Init(void)
{
    /* 软重置。 */
    QMC5883P_I2C_WriteReg(QMC5883P_REG_CFG2, QMC5883P_CFG2_SOFT_RST);
    Delay_ms(2);

    /* CFG2：量程与自动 SET/RESET。 */
    QMC5883P_I2C_WriteReg(QMC5883P_REG_CFG2, 0x20); 
    Delay_ms(1);

    /* CFG1：200Hz 输出速率，连续测量模式。 */
    uint8_t cfg1_val = QMC5883P_CFG1_OSR2_1 | QMC5883P_CFG1_OSR1_8 | 
                       QMC5883P_CFG1_ODR_200HZ | QMC5883P_CFG1_MODE_NORMAL;
    QMC5883P_I2C_WriteReg(QMC5883P_REG_CFG1, cfg1_val);
    Delay_ms(2);

    /* 检查芯片 ID，验证 I2C 通信。 */
    if(QMC5883P_I2C_ReadReg(QMC5883P_REG_CHIP_ID) != 0x80)
    {
        return 1;
    }

    /* 初始化校准偏移。 */
    qmc_x_offset = 0;
    qmc_y_offset = 0;
    return 0;
}

/* 开始校准：重置 X/Y 极值。 */
void QMC5883P_Calibrate_Start(void)
{
    qmc_x_max = -32768; qmc_x_min = 32767;
    qmc_y_max = -32768; qmc_y_min = 32767;
}

/* 校准过程中采集 X/Y 极值。 */
void QMC5883P_Calibrate_Collect(void)
{
    uint8_t buf[6] = {0};
    uint8_t status = 0;

    /* 只采集有效的新数据。 */
    status = QMC5883P_I2C_ReadReg(QMC5883P_REG_STATUS);
    if((status & QMC5883P_STAT_DRDY) == 0) return;
    if((status & QMC5883P_STAT_OVFL) != 0) return;

    /* 读取完整 XYZ 数据，当前校准只使用 X/Y。 */
    QMC5883P_I2C_ReadRegs(QMC5883P_REG_X_LSB, buf, 6);
    int16_t x = (int16_t)(buf[1] << 8) | buf[0];
    int16_t y = (int16_t)(buf[3] << 8) | buf[2];

    /* 更新 X/Y 轴极值。 */
    if(x > qmc_x_max) qmc_x_max = x;
    if(x < qmc_x_min) qmc_x_min = x;
    if(y > qmc_y_max) qmc_y_max = y;
    if(y < qmc_y_min) qmc_y_min = y;
}

/* 结束校准：计算硬铁偏移。 */
void QMC5883P_Calibrate_End(void)
{
    qmc_x_offset = (qmc_x_max + qmc_x_min) / 2;
    qmc_y_offset = (qmc_y_max + qmc_y_min) / 2;
}

/* 对外更新接口。 */
void QMC5883P_UpdateYaw(void)
{
    QMC5883P_ReadXYZ();
}
