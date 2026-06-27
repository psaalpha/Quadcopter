/**
 * AT7456E OSD 驱动 — 硬件 SPI2
 *   PB12 = CS (软件控制)
 *   PB13 = SCK
 *   PB14 = MISO
 *   PB15 = MOSI
 * STM32F103C8 + PAL 制式
 */

#include "AT7456E.h"
#include "Delay.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include <string.h>
#include <stdio.h>

/* 读操作位 */
#define OSD_READ_BIT            0x80

/* CS 引脚宏 */
#define AT7456E_CS_LOW()        GPIO_ResetBits(GPIOB, GPIO_Pin_12)
#define AT7456E_CS_HIGH()       GPIO_SetBits(GPIOB, GPIO_Pin_12)

/*--------------------------------------------------------------------------*/
/*  底层 SPI2 收发一个字节                                                   */
/*--------------------------------------------------------------------------*/
static uint8_t SPI2_SwapByte(uint8_t tx_data)
{
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI2, tx_data);
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET);
    return (uint8_t)SPI_I2S_ReceiveData(SPI2);
}

/*--------------------------------------------------------------------------*/
/*  寄存器读写函数（基于 SPI2）                                              */
/*--------------------------------------------------------------------------*/

/**
 * @brief  向 AT7456E 指定寄存器写入 8 位数据
 */
static void AT7456E_WriteReg(uint8_t addr, uint8_t dat)
{
    AT7456E_CS_LOW();
    SPI2_SwapByte(addr & 0x7F);             // 写地址（最高位为0=写）
    SPI2_SwapByte(dat);                     // 写数据
    AT7456E_CS_HIGH();
}

/**
 * @brief  从 AT7456E 指定寄存器读回 8 位数据
 */
static uint8_t AT7456E_ReadReg(uint8_t addr)
{
    uint8_t dat;

    AT7456E_CS_LOW();
    SPI2_SwapByte(addr | OSD_READ_BIT);     // 读地址（最高位置1）
    Delay_us(5);                            // 至少 5us 延迟
    dat = SPI2_SwapByte(0xFF);              // 发虚字节，读数据
    AT7456E_CS_HIGH();

    return dat;
}

/*--------------------------------------------------------------------------*/
/*  AT7456E 检测与初始化                                                     */
/*--------------------------------------------------------------------------*/

/**
 * @brief  检查 AT7456E 版本（新版/旧版/异常）
 * @return 0=新版, 1=旧版, 2=SPI异常
 */
static uint8_t AT7456E_Check(void)
{
    uint8_t r1, r2;

    r1 = AT7456E_ReadReg(VM0);
    r2 = (r1 & ~(1 << 1)) | 0x88;          // VM0.1(软复位)=0, VM0.3=1, VM0.7=1
    AT7456E_WriteReg(VM0, r2);
    Delay_us(20);
    r2 = AT7456E_ReadReg(VM0) & 0x88;      // 只检查 bit7 和 bit3
    AT7456E_WriteReg(VM0, r1);             // 恢复 VM0
    Delay_us(20);

    if (r2 == 0x88)
        return 0;                           // 新版7456（VM0.7可读写）
    else if (r2 == 0x08)
        return 1;                           // 旧版7456（VM0.7只读）
    else
        return 2;                           // SPI接口异常
}

/**
 * @brief  AT7456E 初始化（SPI2 硬件模式）
 */
void AT7456E_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef  SPI_InitStructure;
    uint8_t k;

    /* ---- 使能时钟 ---- */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    /* ---- CS → PB12 推挽输出，默认高 ---- */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    AT7456E_CS_HIGH();

    /* ---- SCK → PB13, MOSI → PB15 复用推挽 ---- */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_13 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* ---- MISO → PB14 浮空输入 ---- */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* ---- SPI2 配置 ---- */
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;  // 4.5MHz
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_Init(SPI2, &SPI_InitStructure);
    SPI_Cmd(SPI2, ENABLE);

    /* ---- 等待上电复位完成 ---- */
    Delay_ms(60);

    /* ---- 检测芯片 ---- */
    uint8_t version = AT7456E_Check();
    if (version == 2) {
        /* SPI 通信异常，停止初始化 */
        return;
    }

    /* ---- 配置 VM0 视频模式 ---- */
    AT7456E_WriteReg(VM0, 0x42);
    Delay_us(40);

    /* ---- 软件复位后设置 PAL 制式 ---- */
    k = AT7456E_ReadReg(VM0);
    AT7456E_WriteReg(VM0, k | PAL);

    /* ---- 配置 VM1 亮度 ---- */
    k = AT7456E_ReadReg(VM1);
    AT7456E_WriteReg(VM1, (k & 0x8F) | BACKGND_0);

    /* ---- 配置 OSDBL 黑电平 ---- */
    k = AT7456E_ReadReg(OSDBL);
    AT7456E_WriteReg(OSDBL, k & ~(1 << 4));

    /* ---- 清除显存 ---- */
    AT7456E_ClearSRAM();
}

/*--------------------------------------------------------------------------*/
/*  显存操作                                                                  */
/*--------------------------------------------------------------------------*/

/**
 * @brief  清除显存（全部写0）
 */
void AT7456E_ClearSRAM(void)
{
    uint8_t k;
    k = AT7456E_ReadReg(DMM);
    AT7456E_WriteReg(DMM, k | CLEAR_SRAM);  // DMM[2]=1，清除所有显存
    Delay_us(40);                            // 等待清除完成
}

/**
 * @brief  向显存写入一个字符（16位模式）
 * @param  row      行号 0~15（PAL）
 * @param  columns  列号 0~29
 * @param  addr     字符地址 0~255（16位模式下只能访问0~255）
 * @note   写入后需调用 AT7456E_OSD_On() 才能显示
 */
void AT7456E_WriteSRAM(uint8_t row, uint8_t columns, uint8_t addr)
{
    uint16_t pos;

    pos = row * 30 + columns;               // 计算显存地址
    AT7456E_WriteReg(DMAH, pos / 256);      // 地址高位
    AT7456E_WriteReg(DMAL, pos % 256);      // 地址低位
    AT7456E_WriteReg(DMDI, addr);           // 写入字符地址
}

/*--------------------------------------------------------------------------*/
/*  OSD 开关                                                                  */
/*--------------------------------------------------------------------------*/

/**
 * @brief  打开 OSD 显示
 */
void AT7456E_OSD_On(void)
{
    uint8_t k;
    k = AT7456E_ReadReg(VM0);
    AT7456E_WriteReg(VM0, k | OSD_ENABLE);  // VM0[3]=1
    Delay_us(10);
}

/**
 * @brief  关闭 OSD 显示
 */
void AT7456E_OSD_Off(void)
{
    uint8_t k;
    k = AT7456E_ReadReg(VM0);
    AT7456E_WriteReg(VM0, k & ~OSD_ENABLE); // VM0[3]=0
    Delay_us(30);
}

/*--------------------------------------------------------------------------*/
/*  OSD 显示框架初始化                                                        */
/*--------------------------------------------------------------------------*/

/**
 * @brief  OSD 显示框架初始化
 * @note   根据参考代码布局，显示固定字符和框架
 */
void OSD_Init(void)
{
    /* 第0行：模式标识 + 距离 */
    AT7456E_WriteSRAM(0, 0, 0x3C);          // x (模式占位)
    AT7456E_WriteSRAM(0, 1, 0x3C);          // x
    AT7456E_WriteSRAM(0, 2, 0x3C);          // x
    AT7456E_WriteSRAM(0, 3, 0x3C);          // x

    AT7456E_WriteSRAM(0, 4, 0x00);          // 空格
    AT7456E_WriteSRAM(0, 5, 0x0E);          // D (距离标签)
    AT7456E_WriteSRAM(0, 6, 0x44);          // :
    /* 第0行 col7~11 留给距离数值（5位） */
    for (uint8_t i = 7; i <= 11; i++)
        AT7456E_WriteSRAM(0, i, 0x00);      // 清除
    AT7456E_WriteSRAM(0, 12, 0x00);         // 空格

    /* 第1行：光流 X / Y */
    /* 第1行：磁力计航向 */
    AT7456E_WriteSRAM(1, 0, 0x23);          // Y
    AT7456E_WriteSRAM(1, 1, 0x0B);          // A
    AT7456E_WriteSRAM(1, 2, 0x21);          // W
    AT7456E_WriteSRAM(1, 3, 0x44);          // :
    /* 第1行 col4~8 留给航向数值（5位） */
    for (uint8_t i = 4; i <= 8; i++)
        AT7456E_WriteSRAM(1, i, 0x00);

    /* 中间十字准星 */
    AT7456E_WriteSRAM(7, 13, 0x41);         // .
    AT7456E_WriteSRAM(7, 15, 0x41);         // .
    AT7456E_WriteSRAM(6, 14, 0x41);         // .
    AT7456E_WriteSRAM(8, 14, 0x41);         // .

    /* 左边框 */
    AT7456E_WriteSRAM(4, 6, 0x41);          // .
    AT7456E_WriteSRAM(5, 6, 0x41);          // .
    AT7456E_WriteSRAM(6, 6, 0x41);          // .
    AT7456E_WriteSRAM(7, 6, 0x49);          // -
    AT7456E_WriteSRAM(8, 6, 0x41);          // .
    AT7456E_WriteSRAM(9, 6, 0x41);          // .
    AT7456E_WriteSRAM(10, 6, 0x41);         // .
    AT7456E_WriteSRAM(7, 7, 0x4B);          // >

    /* 右边框 */
    AT7456E_WriteSRAM(4, 22, 0x41);         // .
    AT7456E_WriteSRAM(5, 22, 0x41);         // .
    AT7456E_WriteSRAM(6, 22, 0x41);         // .
    AT7456E_WriteSRAM(7, 22, 0x49);         // -
    AT7456E_WriteSRAM(8, 22, 0x41);         // .
    AT7456E_WriteSRAM(9, 22, 0x41);         // .
    AT7456E_WriteSRAM(10, 22, 0x41);        // .
    AT7456E_WriteSRAM(7, 21, 0x4A);         // <

    /* 第13行：T（油门） */
    AT7456E_WriteSRAM(13, 0, 0x1E);         // T

    /* 第14行：P（俯仰）+ 电池电压 */
    AT7456E_WriteSRAM(14, 0, 0x1A);         // P
    AT7456E_WriteSRAM(14, 1, 0x1E);         // V
    AT7456E_WriteSRAM(14, 2, 0x44);         // :

    /* 第15行：R（横滚）+ 核心温度 */
    AT7456E_WriteSRAM(15, 0, 0x1C);         // R
    AT7456E_WriteSRAM(15, 11, 0x1E);        // T
    AT7456E_WriteSRAM(15, 12, 0x44);        // :
    AT7456E_WriteSRAM(15, 17, 0x0D);        // C

    /* DISARMED 状态显示 */
    AT7456E_WriteSRAM(4, 11, 0x0E);         // D
    AT7456E_WriteSRAM(4, 12, 0x13);         // I
    AT7456E_WriteSRAM(4, 13, 0x1D);         // S
    AT7456E_WriteSRAM(4, 14, 0x0B);         // A
    AT7456E_WriteSRAM(4, 15, 0x1C);         // R
    AT7456E_WriteSRAM(4, 16, 0x17);         // M
    AT7456E_WriteSRAM(4, 17, 0x0F);         // E
    AT7456E_WriteSRAM(4, 18, 0x0E);         // D

    /* 打开 OSD 显示 */
    AT7456E_OSD_On();
}

/*--------------------------------------------------------------------------*/
/*  数字显示函数                                                              */
/*--------------------------------------------------------------------------*/

/**
 * @brief  显示整数（高位补0）
 * @param  row         行号
 * @param  columns     起始列号
 * @param  number_len  整数位数（负号占1位）
 * @param  number      要显示的整数
 */
void OSD_DisplayInt(uint8_t row, uint8_t columns, uint8_t number_len, int32_t number)
{
    uint8_t i, k, m;
    uint8_t digits[10] = {0};

    k = 0;
    if (number == 0) {
        /* 全显示0 */
        for (i = 0; i < number_len; i++) {
            m = columns + i;
            AT7456E_WriteSRAM(row, m, 0x0A);    // 显示 '0'
        }
    }
    else if (number > 0) {
        /* 分离各位数字 */
        while (number > 0) {
            digits[k] = number % 10;
            number = number / 10;
            k++;
        }
        if (k > number_len) k = number_len;
        /* 显示 */
        for (i = 0; i < number_len; i++) {
            m = number_len + columns - i - 1;
            if (digits[i] == 0)
                AT7456E_WriteSRAM(row, m, 0x0A);
            else
                AT7456E_WriteSRAM(row, m, digits[i]);
        }
    }
    else {
        /* number < 0 */
        int32_t abs_val = -number;
        while (abs_val > 0) {
            digits[k] = abs_val % 10;
            abs_val = abs_val / 10;
            k++;
        }
        /* 显示负号 */
        AT7456E_WriteSRAM(row, columns, 0x49);   // '-'
        /* 显示数字 */
        for (i = 0; i < (number_len - 1); i++) {
            m = number_len + columns - i - 1;
            if (digits[i] == 0)
                AT7456E_WriteSRAM(row, m, 0x0A);
            else
                AT7456E_WriteSRAM(row, m, digits[i]);
        }
    }
}

/**
 * @brief  显示整数（高位不补0，只留有效数字）
 * @param  row         行号
 * @param  columns     起始列号
 * @param  number_len  整数位数（负号占1位）
 * @param  number      要显示的整数
 */
void OSD_DisplayInt_1(uint8_t row, uint8_t columns, uint8_t number_len, int32_t number)
{
    uint8_t i, k, m;
    uint8_t digits[10] = {0};

    k = 0;
    if (number == 0) {
        /* 最后一位显示0，其余清空 */
        m = columns + number_len - 1;
        AT7456E_WriteSRAM(row, m, 0x0A);        // 末尾显示0
        for (i = 0; i < (number_len - 1); i++) {
            m = columns + i;
            AT7456E_WriteSRAM(row, m, 0x00);    // 清除空位
        }
    }
    else if (number > 0) {
        while (number > 0) {
            digits[k] = number % 10;
            number = number / 10;
            k++;
        }
        if (k > number_len) k = number_len;
        for (i = 0; i < k; i++) {
            m = number_len + columns - 1 - i;
            if (digits[i] == 0)
                AT7456E_WriteSRAM(row, m, 0x0A);
            else
                AT7456E_WriteSRAM(row, m, digits[i]);
        }
        if (k < number_len) {
            for (i = 0; i < (number_len - k); i++) {
                m = columns + i;
                AT7456E_WriteSRAM(row, m, 0x00);
            }
        }
    }
    else {
        /* number < 0 */
        int32_t abs_val = -number;
        while (abs_val > 0) {
            digits[k] = abs_val % 10;
            abs_val = abs_val / 10;
            k++;
        }
        if (k > (number_len - 1)) k = number_len - 1;
        /* 显示数字 */
        for (i = 0; i < k; i++) {
            m = number_len + columns - i - 1;
            if (digits[i] == 0)
                AT7456E_WriteSRAM(row, m, 0x0A);
            else
                AT7456E_WriteSRAM(row, m, digits[i]);
        }
        if (k < (number_len - 1)) {
            for (i = 0; i < (number_len - k - 1); i++) {
                m = columns + i + 1;
                AT7456E_WriteSRAM(row, m, 0x00);
            }
        }
        /* 显示负号 */
        AT7456E_WriteSRAM(row, columns, 0x49);   // '-'
    }
}

/**
 * @brief  显示浮点数
 * @param  row      行号
 * @param  columns  起始列号
 * @param  int_n    整数部分位数（负号占1位）
 * @param  float_n  小数部分位数
 * @param  number   要显示的浮点数
 */
void OSD_DisplayFloat(uint8_t row, uint8_t columns, uint8_t int_n, uint8_t float_n, float number)
{
    uint8_t i, m;
    uint8_t float_digit;        // 小数位数字

    if (number == 0.0f) {
        m = columns + int_n + float_n;
        AT7456E_WriteSRAM(row, m, 0x0A);        // 显示0
    }
    else {
        int32_t int_part;
        float frac_part;

        int_part = (int32_t)number;             // 分离整数部分

        /* 分离小数部分 */
        if (number > 0)
            frac_part = number - (float)int_part;
        else {
            float f_num = -number;
            int32_t i_num = (int32_t)(-number);
            frac_part = f_num - (float)i_num;
        }

        /* 显示小数部分 */
        for (i = 0; i < float_n; i++) {
            frac_part = frac_part * 10.0f;
            float_digit = (uint8_t)frac_part;
            m = columns + int_n + i + 1;
            if (float_digit == 0)
                AT7456E_WriteSRAM(row, m, 0x0A);
            else
                AT7456E_WriteSRAM(row, m, float_digit);
            frac_part = frac_part - (float)float_digit;
        }

        /* 显示小数点 */
        m = columns + int_n;
        AT7456E_WriteSRAM(row, m, 0x41);        // '.'

        /* 显示整数部分 */
        OSD_DisplayInt_1(row, columns, int_n, int_part);

        /* 如果 -1 < number < 0，显示负号 */
        if (number < 0 && number > -1.0f)
            AT7456E_WriteSRAM(row, columns, 0x49);  // '-'
    }
}
