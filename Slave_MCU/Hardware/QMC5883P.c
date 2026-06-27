/**
 * ============================================================
 * QMC5883P 磁力计驱动模块（自包含版）
 *   - 集成软件 I2C 位带操作（PB10=SCL, PB11=SDA）
 *   - 不依赖外部 MyI2C，单一 .c/.h 即可使用
 *   - 对外接口：Init / UpdateYaw / Calibrate_*
 * ============================================================
 */
#include "QMC5883P.h"
#include <math.h>
#include "stm32f10x.h"
#include "stm32f10x_flash.h"
#include "Delay.h"



/* ==================== 全局变量 ==================== */
float   QMC5883P_Yaw      = 0.0f;
float   QMC5883P_Yaw_Last = 0.0f;
int16_t qmc_x_raw = 0;
int16_t qmc_y_raw = 0;
int16_t qmc_z_raw = 0;

/* 硬铁校准参数 */
static int16_t qmc_x_offset = 0;
static int16_t qmc_y_offset = 0;
int16_t qmc_x_max = -32768, qmc_x_min = 32767;
int16_t qmc_y_max = -32768, qmc_y_min = 32767;

#define M_PI  3.1415926535f

/* ==================== Flash 存储（最后一页 0x0800FC00） ==================== */
#define CALIB_FLASH_ADDR    0x0800FC00
#define CALIB_FLASH_MAGIC   0x514D4341   // "QMCA"

/** 校准数据保存到 Flash，成功返回 0，失败返回 1 */
static uint8_t Calib_SaveToFlash(void)
{
    FLASH_Status fs;
    __disable_irq();   // Flash 操作期间必须关全局中断

    FLASH_Unlock();

    /* 清除所有挂起的错误标志，防止之前的状态干扰 */
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    /* 擦除校准页 */
    fs = FLASH_ErasePage(CALIB_FLASH_ADDR);
    if (fs != FLASH_COMPLETE) goto fail;

    /* 写入 Magic 低16位 */
    fs = FLASH_ProgramHalfWord(CALIB_FLASH_ADDR, (uint16_t)(CALIB_FLASH_MAGIC & 0xFFFF));
    if (fs != FLASH_COMPLETE) goto fail;

    /* 写入 Magic 高16位 */
    fs = FLASH_ProgramHalfWord(CALIB_FLASH_ADDR + 2, (uint16_t)(CALIB_FLASH_MAGIC >> 16));
    if (fs != FLASH_COMPLETE) goto fail;

    /* 写入 X 偏移 */
    fs = FLASH_ProgramHalfWord(CALIB_FLASH_ADDR + 4, (uint16_t)qmc_x_offset);
    if (fs != FLASH_COMPLETE) goto fail;

    /* 写入 Y 偏移 */
    fs = FLASH_ProgramHalfWord(CALIB_FLASH_ADDR + 6, (uint16_t)qmc_y_offset);
    if (fs != FLASH_COMPLETE) goto fail;

    FLASH_Lock();
    __enable_irq();
    return 0;   // 成功

fail:
    FLASH_Lock();
    __enable_irq();
    return 1;   // 失败
}

static void Calib_LoadFromFlash(void)
{
    uint32_t magic = (*(__IO uint32_t *)CALIB_FLASH_ADDR);
    if (magic != CALIB_FLASH_MAGIC) return;         // 未校准过

    int16_t x = (int16_t)(*(__IO uint16_t *)(CALIB_FLASH_ADDR + 4));
    int16_t y = (int16_t)(*(__IO uint16_t *)(CALIB_FLASH_ADDR + 6));

    if (x > -30000 && x < 30000 && y > -30000 && y < 30000) {
        qmc_x_offset = x;
        qmc_y_offset = y;
    }
}


/* ==================== 软件 I2C 引脚层（static，模块私有） ==================== */
#define I2C_SCL_PORT  GPIOB
#define I2C_SDA_PORT  GPIOB
#define I2C_SCL_PIN   GPIO_Pin_10
#define I2C_SDA_PIN   GPIO_Pin_11
#define I2C_DELAY_US   10

static void I2C_SCL_Write(uint8_t val)
{
    GPIO_WriteBit(I2C_SCL_PORT, I2C_SCL_PIN, (BitAction)val);
    Delay_us(I2C_DELAY_US);
}

static void I2C_SDA_Write(uint8_t val)
{
    GPIO_WriteBit(I2C_SDA_PORT, I2C_SDA_PIN, (BitAction)val);
    Delay_us(I2C_DELAY_US);
}

static uint8_t I2C_SDA_Read(void)
{
    uint8_t v = GPIO_ReadInputDataBit(I2C_SDA_PORT, I2C_SDA_PIN);
    Delay_us(I2C_DELAY_US);
    return v;
}

/* ==================== 软件 I2C 协议层（static，模块私有） ==================== */
static void I2C_InitGPIO(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitTypeDef cfg;
    cfg.GPIO_Mode  = GPIO_Mode_Out_OD;
    cfg.GPIO_Pin   = I2C_SCL_PIN | I2C_SDA_PIN;
    cfg.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &cfg);
    GPIO_SetBits(GPIOB, I2C_SCL_PIN | I2C_SDA_PIN);   // 释放总线
}

static void I2C_Start(void)
{
    I2C_SDA_Write(1);
    I2C_SCL_Write(1);
    I2C_SDA_Write(0);       // SCL=H 时 SDA 下降沿 → START
    I2C_SCL_Write(0);       // 钳住总线
}

static void I2C_Stop(void)
{
    I2C_SDA_Write(0);
    I2C_SCL_Write(1);
    I2C_SDA_Write(1);       // SCL=H 时 SDA 上升沿 → STOP
}

static void I2C_SendByte(uint8_t dat)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        I2C_SDA_Write(dat & (0x80 >> i));
        I2C_SCL_Write(1);
        I2C_SCL_Write(0);
    }
}

static uint8_t I2C_RecvByte(void)
{
    uint8_t dat = 0;
    I2C_SDA_Write(1);                       // 释放 SDA
    for (uint8_t i = 0; i < 8; i++)
    {
        I2C_SCL_Write(1);
        if (I2C_SDA_Read()) dat |= (0x80 >> i);
        I2C_SCL_Write(0);
    }
    return dat;
}

static void I2C_SendAck(uint8_t ack)
{
    I2C_SDA_Write(ack ? 1 : 0);
    I2C_SCL_Write(1);
    I2C_SCL_Write(0);
}

static uint8_t I2C_RecvAck(void)
{
    uint8_t ack;
    I2C_SDA_Write(1);
    I2C_SCL_Write(1);
    ack = I2C_SDA_Read();
    I2C_SCL_Write(0);
    return ack;             // 0=ACK, 1=NACK
}

/* ==================== QMC5883P 寄存器读写（static 辅助） ==================== */
static void QMC_WriteReg(uint8_t reg, uint8_t dat)
{
    I2C_Start();
    I2C_SendByte(QMC5883P_ADDR_WRITE);  if (I2C_RecvAck()) goto end;
    I2C_SendByte(reg);                  if (I2C_RecvAck()) goto end;
    I2C_SendByte(dat);                  if (I2C_RecvAck()) goto end;
end:
    I2C_Stop();
    Delay_ms(2);
}

static uint8_t QMC_ReadReg(uint8_t reg)
{
    uint8_t dat = 0;
    I2C_Start();
    I2C_SendByte(QMC5883P_ADDR_WRITE);  if (I2C_RecvAck()) goto end;
    I2C_SendByte(reg);                  if (I2C_RecvAck()) goto end;
    I2C_Start();
    I2C_SendByte(QMC5883P_ADDR_READ);   if (I2C_RecvAck()) goto end;
    dat = I2C_RecvByte();
    I2C_SendAck(1);                     // NACK
end:
    I2C_Stop();
    return dat;
}

/** 从 reg 连续读取 len 字节到 buf（通用多字节读取） */
static uint8_t QMC_ReadMulti(uint8_t reg, uint8_t *buf, uint8_t len)
{
    I2C_Start();
    I2C_SendByte(QMC5883P_ADDR_WRITE);  if (I2C_RecvAck()) goto err;
    I2C_SendByte(reg);                  if (I2C_RecvAck()) goto err;
    I2C_Start();
    I2C_SendByte(QMC5883P_ADDR_READ);   if (I2C_RecvAck()) goto err;
    for (uint8_t i = 0; i < len; i++)
    {
        buf[i] = I2C_RecvByte();
        I2C_SendAck((i == len - 1) ? 1 : 0);   // 最后一字节 NACK
    }
    I2C_Stop();
    return 0;   // 成功

err:
    I2C_Stop();
    return 1;   // 失败
}

/* ==================== 核心：读取 XYZ 并计算航向角 ==================== */
static void QMC_ReadXYZ(void)
{
    uint8_t buf[6], status;

    /* 只查一次 DRDY（传感器 10Hz，轮询 20Hz，未就绪直接返回用上次数据） */
    status = QMC_ReadReg(QMC5883P_REG_STATUS);
    if (!(status & QMC5883P_STAT_DRDY)) return;
    if (status & QMC5883P_STAT_OVFL)    return;

    /* 连续读 6 字节 */
    if (QMC_ReadMulti(QMC5883P_REG_X_LSB, buf, 6)) return;

    /* 组合为有符号 16 位 */
    int16_t x = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t y = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t z = (int16_t)((buf[5] << 8) | buf[4]);

    if (x == 0 && y == 0) return;           // 过滤无效数据

    /* 应用硬铁校准偏移 */
    qmc_x_raw = x - qmc_x_offset;
    qmc_y_raw = y - qmc_y_offset;
    qmc_z_raw = z;

    /* 计算航向角 0~360° */
    QMC5883P_Yaw = atan2f((float)qmc_y_raw, (float)qmc_x_raw) * (180.0f / M_PI);
    if (QMC5883P_Yaw < 0.0f) QMC5883P_Yaw += 360.0f;
    QMC5883P_Yaw_Last = QMC5883P_Yaw;
}

/* ==================== 对外接口 ==================== */

uint8_t QMC5883P_Init(void)
{
    /* 1. GPIO 初始化（原 MyI2C_Init 内容） */
    I2C_InitGPIO();

    /* 2. 软重置 */
    QMC_WriteReg(QMC5883P_REG_CFG2, QMC5883P_CFG2_SOFT_RST);
    Delay_ms(10);

    /* 3. CFG2：±2G 量程 + 自动 SET/RESET */
    QMC_WriteReg(QMC5883P_REG_CFG2, QMC5883P_CFG2_RNG_2G | QMC5883P_CFG2_SET_RESET);
    Delay_ms(5);

    /* 4. CFG1：连续模式 + 10Hz + OSR=8 */
    QMC_WriteReg(QMC5883P_REG_CFG1,
        QMC5883P_CFG1_OSR2_1 | QMC5883P_CFG1_OSR1_8 |
        QMC5883P_CFG1_ODR_10HZ | QMC5883P_CFG1_MODE_NORMAL);
    Delay_ms(20);

    /* 5. 验证芯片 ID */
    if (QMC_ReadReg(QMC5883P_REG_CHIP_ID) != 0x80) return 1;

    /* 6. 从 Flash 加载校准参数 */
    Calib_LoadFromFlash();

    return 0;   // 成功
}

void QMC5883P_UpdateYaw(void)
{
    QMC_ReadXYZ();
}

void QMC5883P_Calibrate_Start(void)
{
    qmc_x_max = -32768; qmc_x_min = 32767;
    qmc_y_max = -32768; qmc_y_min = 32767;
}

void QMC5883P_Calibrate_Collect(void)
{
    uint8_t buf[4];

    if (QMC_ReadMulti(QMC5883P_REG_X_LSB, buf, 4)) return;   // 复用通用读取

    int16_t x = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t y = (int16_t)((buf[3] << 8) | buf[2]);

    if (x > qmc_x_max) qmc_x_max = x;
    if (x < qmc_x_min) qmc_x_min = x;
    if (y > qmc_y_max) qmc_y_max = y;
    if (y < qmc_y_min) qmc_y_min = y;
}

uint8_t QMC5883P_Calibrate_End(void)
{
    qmc_x_offset = (qmc_x_max + qmc_x_min) / 2;
    qmc_y_offset = (qmc_y_max + qmc_y_min) / 2;

    /* 写入 Flash，失败返回 1 */
    if (Calib_SaveToFlash() != 0) return 1;

    /* 回读验证：确认 Flash 数据与 RAM 一致 */
    Calib_LoadFromFlash();
    if (qmc_x_offset != (qmc_x_max + qmc_x_min) / 2 ||
        qmc_y_offset != (qmc_y_max + qmc_y_min) / 2) {
        return 1;
    }

    return 0;   // 校准 + 保存成功
}
