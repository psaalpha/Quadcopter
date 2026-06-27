#include "QMC5883P.h"
#include <math.h>
#include "stm32f10x.h"
#include "MyI2C.h"   
#include "Delay.h"

// 全局变量定义
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
#define FILTER_WINDOW 5  // 5点滑动滤波，降低噪声
float yaw_filter_buf[FILTER_WINDOW] = {0.0f};
uint8_t filter_idx = 0;

// 内部函数：单字节写寄存器（保留）
static void QMC5883P_I2C_WriteReg(uint8_t reg, uint8_t data)
{
    MyI2C_Start();
    MyI2C_SendByte(QMC5883P_ADDR_WRITE);
    if(MyI2C_ReceiveAck() != 0) return; // 增加ACK检查，防止I2C挂死
    MyI2C_SendByte(reg);
    if(MyI2C_ReceiveAck() != 0) return;
    MyI2C_SendByte(data);
    if(MyI2C_ReceiveAck() != 0) return;
    MyI2C_Stop();
}

// 内部函数：读单个寄存器（新增，用于状态检查）
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
    MyI2C_SendAck(1); // 非应答
    MyI2C_Stop();
    return data;
}

// 内部函数：连续读多个寄存器（核心，替代单字节读）
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
        if(i < len-1) MyI2C_SendAck(0); // 前n-1个发ACK
        else MyI2C_SendAck(1);          // 最后1个发NACK
    }
    MyI2C_Stop();
}

// 内部函数：读取XYZ原始数据（严格按手册，增加状态检查）
static void QMC5883P_ReadXYZ(void)
{
    uint8_t buf[6] = {0};
    uint8_t status = 0;

    // 步骤1：检查状态寄存器，确保数据就绪且无溢出（手册9.2.2节）
    status = QMC5883P_I2C_ReadReg(QMC5883P_REG_STATUS);
    if((status & QMC5883P_STAT_DRDY) == 0) return; // 无新数据，直接返回
    if((status & QMC5883P_STAT_OVFL) != 0) return; // 数据溢出，直接返回

    // 步骤2：连续读取6字节XYZ数据（手册9.1节，0x01~0x06）
    QMC5883P_I2C_ReadRegs(QMC5883P_REG_X_LSB, buf, 6);

    // 步骤3：拼接16位数据（大端模式，MSB在后，手册9.2.1节）
    int16_t x = (int16_t)(buf[1] << 8) | buf[0];
    int16_t y = (int16_t)(buf[3] << 8) | buf[2];
    int16_t z = (int16_t)(buf[5] << 8) | buf[4];

    // 步骤4：校准偏移（消除硬铁干扰）
    qmc_x_raw = x - qmc_x_offset;
    qmc_y_raw = y - qmc_y_offset;
    qmc_z_raw = z;

    // 步骤5：计算航向角（右手坐标系，手册推荐atan2(Y,X)）
    float raw_yaw = atan2((float)qmc_y_raw, (float)qmc_x_raw) * 180.0f / M_PI;
    if(raw_yaw < 0) raw_yaw += 360.0f; // 转换为0~360°

    // 步骤6：滑动滤波（降低噪声，平滑航向角）
    yaw_filter_buf[filter_idx++] = raw_yaw;
    if(filter_idx >= FILTER_WINDOW) filter_idx = 0;
    QMC5883P_Yaw = 0.0f;
    for(uint8_t i=0; i<FILTER_WINDOW; i++)
    {
        QMC5883P_Yaw += yaw_filter_buf[i];
    }
    QMC5883P_Yaw /= FILTER_WINDOW;
    QMC5883P_Yaw_Last = QMC5883P_Yaw; // 更新上一次航向角
}

// 外部函数：QMC5883P初始化（严格按手册5.3/6.2节，核心修复）
uint8_t QMC5883P_Init(void)
{
    // 步骤1：软重置（手册7.6节，写0x80到CFG2(0x0B)）
    QMC5883P_I2C_WriteReg(QMC5883P_REG_CFG2, QMC5883P_CFG2_SOFT_RST);
    Delay_ms(2); // 手册要求POR完成时间最大250us，留余量

    // 步骤2：配置CFG2（0x0B）：关闭软重置+±8G量程+自动SET/RESET（手册9.2.3节）
    // ±8G量程（3750LSB/G），兼顾灵敏度和抗干扰，比默认±30G优
    QMC5883P_I2C_WriteReg(QMC5883P_REG_CFG2, 0x20); 
    Delay_ms(1);

    // 步骤3：配置CFG1（0x0A）：OSR2=1+OSR1=8+200Hz ODR+正常模式（手册9.2.3节Table17）
    // 0x0A = 00(OSR2) + 00(OSR1) + 0C(200Hz ODR) + 01(正常模式) = 0x0D
    uint8_t cfg1_val = QMC5883P_CFG1_OSR2_1 | QMC5883P_CFG1_OSR1_8 | 
                       QMC5883P_CFG1_ODR_200HZ | QMC5883P_CFG1_MODE_NORMAL;
    QMC5883P_I2C_WriteReg(QMC5883P_REG_CFG1, cfg1_val);
    Delay_ms(2);

    // 步骤4：检查芯片ID（手册9.2.1节，0x00寄存器默认0x80），验证I2C通信
    if(QMC5883P_I2C_ReadReg(QMC5883P_REG_CHIP_ID) != 0x80)
    {
        return 1; // ID错误，初始化失败
    }

    // 步骤5：初始化校准偏移
    qmc_x_offset = 0;
    qmc_y_offset = 0;
    return 0; // 初始化成功
}

// 外部函数：校准开始（重置极值）
void QMC5883P_Calibrate_Start(void)
{
    qmc_x_max = -32768; qmc_x_min = 32767;
    qmc_y_max = -32768; qmc_y_min = 32767;
}

// 外部函数：校准数据收集（修复：读6字节+状态检查，手册规范）
void QMC5883P_Calibrate_Collect(void)
{
    uint8_t buf[6] = {0};
    uint8_t status = 0;

    // 先检查数据就绪，避免采集旧数据
    status = QMC5883P_I2C_ReadReg(QMC5883P_REG_STATUS);
    if((status & QMC5883P_STAT_DRDY) == 0) return;
    if((status & QMC5883P_STAT_OVFL) != 0) return;

    // 读取6字节完整XYZ数据，校准更精准
    QMC5883P_I2C_ReadRegs(QMC5883P_REG_X_LSB, buf, 6);
    int16_t x = (int16_t)(buf[1] << 8) | buf[0];
    int16_t y = (int16_t)(buf[3] << 8) | buf[2];

    // 更新X/Y轴极值（硬铁校准仅需X/Y）
    if(x > qmc_x_max) qmc_x_max = x;
    if(x < qmc_x_min) qmc_x_min = x;
    if(y > qmc_y_max) qmc_y_max = y;
    if(y < qmc_y_min) qmc_y_min = y;
}

// 外部函数：校准结束（计算硬铁偏移，不变）
void QMC5883P_Calibrate_End(void)
{
    qmc_x_offset = (qmc_x_max + qmc_x_min) / 2;
    qmc_y_offset = (qmc_y_max + qmc_y_min) / 2;
}

// 外部函数：更新航向角（对外接口，仅调用内部读取函数）
void QMC5883P_UpdateYaw(void)
{
    QMC5883P_ReadXYZ();
}
