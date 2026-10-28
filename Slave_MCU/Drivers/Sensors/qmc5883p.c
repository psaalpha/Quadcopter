/**
 * ============================================================
 * QMC5883P 磁力计驱动模块（自包含版）
 *   - 集成软件 I2C 位带操作（PB10=SCL, PB11=SDA）
 *   - 不依赖外部 soft_i2c，单一 .c/.h 即可使用
 *   - 对外接口：Init / UpdateYaw / Calibrate_*
 * ============================================================
 */
#include "qmc5883p.h"
#include <math.h>
#include "stm32f10x.h"
#include "stm32f10x_flash.h"
#include "Delay.h"
#include "flash_config.h"



/* ==================== 全局变量 ==================== */
float   qmc5883p_yaw_deg      = 0.0f;
float   qmc5883p_last_yaw_deg = 0.0f;
int16_t qmc_x_raw = 0;
int16_t qmc_y_raw = 0;
int16_t qmc_z_raw = 0;

/* 硬铁校准参数 */
static int16_t qmc_x_offset = 0;
static int16_t qmc_y_offset = 0;
static int16_t qmc_z_offset = 0;
static float qmc_scale[3]={1.0f,1.0f,1.0f};
static int16_t qmc_z_max=-32768,qmc_z_min=32767;
int16_t qmc_x_max = -32768, qmc_x_min = 32767;
int16_t qmc_y_max = -32768, qmc_y_min = 32767;

#define M_PI  3.1415926535f
static uint16_t calibration_samples;

/* 双页参数存储；以下地址仅用于兼容旧版两轴校准记录。 */
#define CALIB_FLASH_ADDR    0x0800FC00
#define CALIB_FLASH_MAGIC   0x514D4341   // "QMCA"

/** 校准数据保存到 Flash，成功返回 0，失败返回 1 */
static uint8_t qmc5883p_calib_save_to_flash(void)
{
    float image[6]={(float)qmc_x_offset,(float)qmc_y_offset,(float)qmc_z_offset,
        qmc_scale[0],qmc_scale[1],qmc_scale[2]};
    return flash_config_save(0x2001u,image,sizeof(image))?0u:1u;
}
static void qmc5883p_calib_load_from_flash(void)
{
    float image[6];unsigned i;
    if(flash_config_load(0x2001u,image,sizeof(image))) {
        for(i=0u;i<3u;i++)if(!(image[i]>-30000 && image[i]<30000 && image[3u+i]>=.25f && image[3u+i]<=4))return;
        qmc_x_offset=(int16_t)image[0];qmc_y_offset=(int16_t)image[1];qmc_z_offset=(int16_t)image[2];
        for(i=0u;i<3u;i++)qmc_scale[i]=image[3u+i];
    } else if(*(__IO uint32_t *)CALIB_FLASH_ADDR==CALIB_FLASH_MAGIC) {
        /* Read the previous two-axis layout once; a new save uses two pages. */
        int16_t x=(int16_t)*(__IO uint16_t *)(CALIB_FLASH_ADDR+4u);
        int16_t y=(int16_t)*(__IO uint16_t *)(CALIB_FLASH_ADDR+6u);
        if(x>-30000 && x<30000 && y>-30000 && y<30000){qmc_x_offset=x;qmc_y_offset=y;}
    }
}
static int16_t calibrated_axis(int16_t raw,int16_t offset,float scale)
{
    float value=((float)raw-(float)offset)*scale;
    if(value>32767)value=32767;
    if(value< -32768)value=-32768;
    return (int16_t)value;
}

/* ==================== 软件 I2C 引脚层（static，模块私有） ==================== */
#define I2C_SCL_PORT  GPIOB
#define I2C_SDA_PORT  GPIOB
#define I2C_SCL_PIN   GPIO_Pin_10
#define I2C_SDA_PIN   GPIO_Pin_11
#define I2C_DELAY_US   10

static void qmc5883p_i2c_scl_write(uint8_t val)
{
    GPIO_WriteBit(I2C_SCL_PORT, I2C_SCL_PIN, (BitAction)val);
    Delay_us(I2C_DELAY_US);
}

static void qmc5883p_i2c_sda_write(uint8_t val)
{
    GPIO_WriteBit(I2C_SDA_PORT, I2C_SDA_PIN, (BitAction)val);
    Delay_us(I2C_DELAY_US);
}

static uint8_t qmc5883p_i2c_sda_read(void)
{
    uint8_t v = GPIO_ReadInputDataBit(I2C_SDA_PORT, I2C_SDA_PIN);
    Delay_us(I2C_DELAY_US);
    return v;
}

/* ==================== 软件 I2C 协议层（static，模块私有） ==================== */
static void qmc5883p_i2c_init_gpio(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitTypeDef cfg;
    cfg.GPIO_Mode  = GPIO_Mode_Out_OD;
    cfg.GPIO_Pin   = I2C_SCL_PIN | I2C_SDA_PIN;
    cfg.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &cfg);
    GPIO_SetBits(GPIOB, I2C_SCL_PIN | I2C_SDA_PIN);   // 释放总线
}

static void qmc5883p_i2c_start(void)
{
    qmc5883p_i2c_sda_write(1);
    qmc5883p_i2c_scl_write(1);
    qmc5883p_i2c_sda_write(0);       // SCL=H 时 SDA 下降沿 → START
    qmc5883p_i2c_scl_write(0);       // 钳住总线
}

static void qmc5883p_i2c_stop(void)
{
    qmc5883p_i2c_sda_write(0);
    qmc5883p_i2c_scl_write(1);
    qmc5883p_i2c_sda_write(1);       // SCL=H 时 SDA 上升沿 → STOP
}

static void qmc5883p_i2c_send_byte(uint8_t dat)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        qmc5883p_i2c_sda_write(dat & (0x80 >> i));
        qmc5883p_i2c_scl_write(1);
        qmc5883p_i2c_scl_write(0);
    }
}

static uint8_t qmc5883p_i2c_recv_byte(void)
{
    uint8_t dat = 0;
    qmc5883p_i2c_sda_write(1);                       // 释放 SDA
    for (uint8_t i = 0; i < 8; i++)
    {
        qmc5883p_i2c_scl_write(1);
        if (qmc5883p_i2c_sda_read()) dat |= (0x80 >> i);
        qmc5883p_i2c_scl_write(0);
    }
    return dat;
}

static void qmc5883p_i2c_send_ack(uint8_t ack)
{
    qmc5883p_i2c_sda_write(ack ? 1 : 0);
    qmc5883p_i2c_scl_write(1);
    qmc5883p_i2c_scl_write(0);
}

static uint8_t qmc5883p_i2c_recv_ack(void)
{
    uint8_t ack;
    qmc5883p_i2c_sda_write(1);
    qmc5883p_i2c_scl_write(1);
    ack = qmc5883p_i2c_sda_read();
    qmc5883p_i2c_scl_write(0);
    return ack;             // 0=ACK, 1=NACK
}

/* ==================== QMC5883P 寄存器读写（static 辅助） ==================== */
static void qmc5883p_write_reg(uint8_t reg, uint8_t dat)
{
    qmc5883p_i2c_start();
    qmc5883p_i2c_send_byte(QMC5883P_ADDR_WRITE);  if (qmc5883p_i2c_recv_ack()) goto end;
    qmc5883p_i2c_send_byte(reg);                  if (qmc5883p_i2c_recv_ack()) goto end;
    qmc5883p_i2c_send_byte(dat);                  if (qmc5883p_i2c_recv_ack()) goto end;
end:
    qmc5883p_i2c_stop();
    Delay_ms(2);
}

static uint8_t qmc5883p_read_reg(uint8_t reg)
{
    uint8_t dat = 0;
    qmc5883p_i2c_start();
    qmc5883p_i2c_send_byte(QMC5883P_ADDR_WRITE);  if (qmc5883p_i2c_recv_ack()) goto end;
    qmc5883p_i2c_send_byte(reg);                  if (qmc5883p_i2c_recv_ack()) goto end;
    qmc5883p_i2c_start();
    qmc5883p_i2c_send_byte(QMC5883P_ADDR_READ);   if (qmc5883p_i2c_recv_ack()) goto end;
    dat = qmc5883p_i2c_recv_byte();
    qmc5883p_i2c_send_ack(1);                     // NACK
end:
    qmc5883p_i2c_stop();
    return dat;
}

/** 从 reg 连续读取 len 字节到 buf（通用多字节读取） */
static uint8_t qmc5883p_read_multi(uint8_t reg, uint8_t *buf, uint8_t len)
{
    qmc5883p_i2c_start();
    qmc5883p_i2c_send_byte(QMC5883P_ADDR_WRITE);  if (qmc5883p_i2c_recv_ack()) goto err;
    qmc5883p_i2c_send_byte(reg);                  if (qmc5883p_i2c_recv_ack()) goto err;
    qmc5883p_i2c_start();
    qmc5883p_i2c_send_byte(QMC5883P_ADDR_READ);   if (qmc5883p_i2c_recv_ack()) goto err;
    for (uint8_t i = 0; i < len; i++)
    {
        buf[i] = qmc5883p_i2c_recv_byte();
        qmc5883p_i2c_send_ack((i == len - 1) ? 1 : 0);   // 最后一字节 NACK
    }
    qmc5883p_i2c_stop();
    return 0;   // 成功

err:
    qmc5883p_i2c_stop();
    return 1;   // 失败
}

/* ==================== 核心：读取 XYZ 并计算航向角 ==================== */
static uint8_t qmc5883p_read_xyz(void)
{
    uint8_t buf[6], status;

    /* 只查一次 DRDY（传感器 10Hz，轮询 20Hz，未就绪直接返回用上次数据） */
    status = qmc5883p_read_reg(QMC5883P_REG_STATUS);
    if (!(status & QMC5883P_STAT_DRDY)) return 0u;
    if (status & QMC5883P_STAT_OVFL)    return 0u;

    /* 连续读 6 字节 */
    if (qmc5883p_read_multi(QMC5883P_REG_X_LSB, buf, 6)) return 0u;

    /* 组合为有符号 16 位 */
    int16_t x = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t y = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t z = (int16_t)((buf[5] << 8) | buf[4]);

    if (x == 0 && y == 0) return 0u;        // 过滤无效数据

    /* 应用硬铁校准偏移 */
    qmc_x_raw = calibrated_axis(x,qmc_x_offset,qmc_scale[0]);
    qmc_y_raw = calibrated_axis(y,qmc_y_offset,qmc_scale[1]);
    qmc_z_raw = calibrated_axis(z,qmc_z_offset,qmc_scale[2]);

    /* 计算航向角 0~360° */
    qmc5883p_yaw_deg = atan2f((float)qmc_y_raw, (float)qmc_x_raw) * (180.0f / M_PI);
    if (qmc5883p_yaw_deg < 0.0f) qmc5883p_yaw_deg += 360.0f;
    qmc5883p_last_yaw_deg = qmc5883p_yaw_deg;
    return 1u;
}

/* ==================== 对外接口 ==================== */

uint8_t qmc5883p_init(void)
{
    /* 1. GPIO 初始化（原 soft_i2c_init 内容） */
    qmc5883p_i2c_init_gpio();

    /* 2. 软重置 */
    qmc5883p_write_reg(QMC5883P_REG_CFG2, QMC5883P_CFG2_SOFT_RST);
    Delay_ms(10);

    /* 3. CFG2：±2G 量程 + 自动 SET/RESET */
    qmc5883p_write_reg(QMC5883P_REG_CFG2, QMC5883P_CFG2_RNG_2G | QMC5883P_CFG2_SET_RESET);
    Delay_ms(5);

    /* 4. CFG1：连续模式 + 10Hz + OSR=8 */
    qmc5883p_write_reg(QMC5883P_REG_CFG1,
        QMC5883P_CFG1_OSR2_1 | QMC5883P_CFG1_OSR1_8 |
        QMC5883P_CFG1_ODR_10HZ | QMC5883P_CFG1_MODE_NORMAL);
    Delay_ms(20);

    /* 5. 验证芯片 ID */
    if (qmc5883p_read_reg(QMC5883P_REG_CHIP_ID) != 0x80) return 1;

    /* 6. 从 Flash 加载校准参数 */
    qmc5883p_calib_load_from_flash();

    return 0;   // 成功
}

uint8_t qmc5883p_update_yaw(void)
{
    return qmc5883p_read_xyz();
}

void qmc5883p_calibrate_start(void)
{
    calibration_samples = 0u;
    qmc_x_max = -32768; qmc_x_min = 32767;
    qmc_y_max = -32768; qmc_y_min = 32767;
    qmc_z_max = -32768; qmc_z_min = 32767;
}

void qmc5883p_calibrate_collect(void)
{
    uint8_t buf[6];
    uint8_t status = qmc5883p_read_reg(QMC5883P_REG_STATUS);
    if ((status & QMC5883P_STAT_DRDY) == 0u || (status & QMC5883P_STAT_OVFL) != 0u) return;

    if (qmc5883p_read_multi(QMC5883P_REG_X_LSB, buf, 6)) return;   // 复用通用读取

    int16_t x = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t y = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t z = (int16_t)((buf[5] << 8) | buf[4]);

    calibration_samples++;
    if (x > qmc_x_max) qmc_x_max = x;
    if (x < qmc_x_min) qmc_x_min = x;
    if (y > qmc_y_max) qmc_y_max = y;
    if (y < qmc_y_min) qmc_y_min = y;
    if (z > qmc_z_max) qmc_z_max = z;
    if (z < qmc_z_min) qmc_z_min = z;
}

uint8_t qmc5883p_calibrate_end(void)
{
    float radius[3],average,old_scale[3];unsigned i;
    int16_t previous[3]={qmc_x_offset,qmc_y_offset,qmc_z_offset};
    radius[0]=((float)qmc_x_max-(float)qmc_x_min)*.5f;
    radius[1]=((float)qmc_y_max-(float)qmc_y_min)*.5f;
    radius[2]=((float)qmc_z_max-(float)qmc_z_min)*.5f;
    if(calibration_samples<40u || radius[0]<50 || radius[1]<50 || radius[2]<50)return 1u;
    average=(radius[0]+radius[1]+radius[2])/3.0f;
    for(i=0u;i<3u;i++)if(average/radius[i]<.25f || average/radius[i]>4)return 1u;
    qmc_x_offset=(int16_t)(((int32_t)qmc_x_max+qmc_x_min)/2);
    qmc_y_offset=(int16_t)(((int32_t)qmc_y_max+qmc_y_min)/2);
    qmc_z_offset=(int16_t)(((int32_t)qmc_z_max+qmc_z_min)/2);
    if(qmc_x_offset<=-30000 || qmc_x_offset>=30000 || qmc_y_offset<=-30000 ||
       qmc_y_offset>=30000 || qmc_z_offset<=-30000 || qmc_z_offset>=30000) {
        qmc_x_offset=previous[0];qmc_y_offset=previous[1];qmc_z_offset=previous[2];return 1u;
    }
    for(i=0u;i<3u;i++){old_scale[i]=qmc_scale[i];qmc_scale[i]=average/radius[i];}
    if(qmc5883p_calib_save_to_flash()!=0u) {
        qmc_x_offset=previous[0];qmc_y_offset=previous[1];qmc_z_offset=previous[2];
        for(i=0u;i<3u;i++)qmc_scale[i]=old_scale[i];
        return 1u;
    }
    return 0u;
}
