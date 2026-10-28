/**
 * AT7456E OSD 驱动 — 硬件 SPI2
 *   PB12 = CS (软件控制)
 *   PB13 = SCK
 *   PB14 = MISO
 *   PB15 = MOSI
 * STM32F103C8 + AT7456E_PAL 制式
 */

#include "at7456e.h"
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
static uint32_t spi_timeouts;
uint32_t at7456e_get_spi_timeouts(void) { return spi_timeouts; }
static uint8_t at7456e_spi_transfer(uint8_t tx_data)
{
    uint32_t timeout = 2000u;
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET && timeout != 0u) timeout--;
    if (timeout == 0u) { spi_timeouts++; return 0xFFu; }
    SPI_I2S_SendData(SPI2, tx_data);
    timeout = 2000u;
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET && timeout != 0u) timeout--;
    if (timeout == 0u) { spi_timeouts++; return 0xFFu; }
    return (uint8_t)SPI_I2S_ReceiveData(SPI2);
}

/*--------------------------------------------------------------------------*/
/*  寄存器读写函数（基于 SPI2）                                              */
/*--------------------------------------------------------------------------*/

/**
 * @brief  向 AT7456E 指定寄存器写入 8 位数据
 */
static void at7456e_write_reg(uint8_t addr, uint8_t dat)
{
    AT7456E_CS_LOW();
    at7456e_spi_transfer(addr & 0x7F);             // 写地址（最高位为0=写）
    at7456e_spi_transfer(dat);                     // 写数据
    AT7456E_CS_HIGH();
}

/**
 * @brief  从 AT7456E 指定寄存器读回 8 位数据
 */
static uint8_t at7456e_read_reg(uint8_t addr)
{
    uint8_t dat;

    AT7456E_CS_LOW();
    at7456e_spi_transfer(addr | OSD_READ_BIT);     // 读地址（最高位置1）
    Delay_us(5);                            // 至少 5us 延迟
    dat = at7456e_spi_transfer(0xFF);              // 发虚字节，读数据
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
static uint8_t at7456e_check(void)
{
    uint8_t r1, r2;

    r1 = at7456e_read_reg(AT7456E_VM0);
    r2 = (r1 & ~(1 << 1)) | 0x88;          // AT7456E_VM0.1(软复位)=0, AT7456E_VM0.3=1, AT7456E_VM0.7=1
    at7456e_write_reg(AT7456E_VM0, r2);
    Delay_us(20);
    r2 = at7456e_read_reg(AT7456E_VM0) & 0x88;      // 只检查 bit7 和 bit3
    at7456e_write_reg(AT7456E_VM0, r1);             // 恢复 AT7456E_VM0
    Delay_us(20);

    if (r2 == 0x88)
        return 0;                           // 新版7456（AT7456E_VM0.7可读写）
    else if (r2 == 0x08)
        return 1;                           // 旧版7456（AT7456E_VM0.7只读）
    else
        return 2;                           // SPI接口异常
}

/**
 * @brief  AT7456E 初始化（SPI2 硬件模式）
 */
void at7456e_init(void)
{
    GPIO_InitTypeDef gpio_config;
    SPI_InitTypeDef  SPI_InitStructure;
    uint8_t k;

    /* ---- 使能时钟 ---- */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    /* ---- CS → PB12 推挽输出，默认高 ---- */
    gpio_config.GPIO_Pin   = GPIO_Pin_12;
    gpio_config.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio_config.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio_config);
    AT7456E_CS_HIGH();

    /* ---- SCK → PB13, MOSI → PB15 复用推挽 ---- */
    gpio_config.GPIO_Pin  = GPIO_Pin_13 | GPIO_Pin_15;
    gpio_config.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &gpio_config);

    /* ---- MISO → PB14 浮空输入 ---- */
    gpio_config.GPIO_Pin  = GPIO_Pin_14;
    gpio_config.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio_config);

    /* ---- SPI2 配置 ---- */
    SPI_StructInit(&SPI_InitStructure);
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
    uint8_t version = at7456e_check();
    if (version == 2) {
        /* SPI 通信异常，停止初始化 */
        return;
    }

    /* ---- 配置 AT7456E_VM0 视频模式 ---- */
    at7456e_write_reg(AT7456E_VM0, 0x42);
    Delay_us(40);

    /* ---- 软件复位后设置 AT7456E_PAL 制式 ---- */
    k = at7456e_read_reg(AT7456E_VM0);
    at7456e_write_reg(AT7456E_VM0, k | AT7456E_PAL);

    /* ---- 配置 AT7456E_VM1 亮度 ---- */
    k = at7456e_read_reg(AT7456E_VM1);
    at7456e_write_reg(AT7456E_VM1, (k & 0x8F) | AT7456E_BACKGND_0);

    /* ---- 配置 AT7456E_OSDBL 黑电平 ---- */
    k = at7456e_read_reg(AT7456E_OSDBL);
    at7456e_write_reg(AT7456E_OSDBL, k & ~(1 << 4));

    /* ---- 清除显存 ---- */
    at7456e_clear_sram();
}

/*--------------------------------------------------------------------------*/
/*  显存操作                                                                  */
/*--------------------------------------------------------------------------*/

/**
 * @brief  清除显存（全部写0）
 */
void at7456e_clear_sram(void)
{
    uint8_t k;
    k = at7456e_read_reg(AT7456E_DMM);
    at7456e_write_reg(AT7456E_DMM, k | AT7456E_CLEAR_SRAM);  // AT7456E_DMM[2]=1，清除所有显存
    Delay_us(40);                            // 等待清除完成
}

/**
 * @brief  向显存写入一个字符（16位模式）
 * @param  row      行号 0~15（AT7456E_PAL）
 * @param  columns  列号 0~29
 * @param  addr     字符地址 0~255（16位模式下只能访问0~255）
 * @note   写入后需调用 at7456e_osd_on() 才能显示
 */
void at7456e_write_sram(uint8_t row, uint8_t columns, uint8_t addr)
{
    uint16_t pos;

    pos = row * 30 + columns;               // 计算显存地址
    at7456e_write_reg(AT7456E_DMAH, pos / 256);      // 地址高位
    at7456e_write_reg(AT7456E_DMAL, pos % 256);      // 地址低位
    at7456e_write_reg(AT7456E_DMDI, addr);           // 写入字符地址
}

/*--------------------------------------------------------------------------*/
/*  OSD 开关                                                                  */
/*--------------------------------------------------------------------------*/

/**
 * @brief  打开 OSD 显示
 */
void at7456e_osd_on(void)
{
    uint8_t k;
    k = at7456e_read_reg(AT7456E_VM0);
    at7456e_write_reg(AT7456E_VM0, k | AT7456E_OSD_ENABLE);  // AT7456E_VM0[3]=1
    Delay_us(10);
}

/**
 * @brief  关闭 OSD 显示
 */
void at7456e_osd_off(void)
{
    uint8_t k;
    k = at7456e_read_reg(AT7456E_VM0);
    at7456e_write_reg(AT7456E_VM0, k & ~AT7456E_OSD_ENABLE); // AT7456E_VM0[3]=0
    Delay_us(30);
}

/*--------------------------------------------------------------------------*/
/*  OSD 显示框架初始化                                                        */
/*--------------------------------------------------------------------------*/

/**
 * @brief  OSD 显示框架初始化
 * @note   根据参考代码布局，显示固定字符和框架
 */
void osd_init(void)
{
    /* Optical-flow quality (0..100); no static flight-mode claim. */
    for(uint8_t i=0;i<4u;i++)at7456e_write_sram(0,i,0x00);
    at7456e_write_sram(0, 4, 0x00);          // 空格
    at7456e_write_sram(0, 5, 0x0E);          // D (距离标签)
    at7456e_write_sram(0, 6, 0x44);          // :
    /* 第0行 col7~11 留给距离数值（5位） */
    for (uint8_t i = 7; i <= 11; i++)
        at7456e_write_sram(0, i, 0x00);      // 清除
    at7456e_write_sram(0, 12, 0x00);         // 空格

    /* 第1行：磁力计航向 */
    at7456e_write_sram(1, 0, 0x23);          // Y
    at7456e_write_sram(1, 1, 0x0B);          // A
    at7456e_write_sram(1, 2, 0x21);          // W
    at7456e_write_sram(1, 3, 0x44);          // :
    /* 第1行 col4~8 留给航向数值（5位） */
    for (uint8_t i = 4; i <= 8; i++)
        at7456e_write_sram(1, i, 0x00);

    /* 中间十字准星 */
    at7456e_write_sram(7, 13, 0x41);         // .
    at7456e_write_sram(7, 15, 0x41);         // .
    at7456e_write_sram(6, 14, 0x41);         // .
    at7456e_write_sram(8, 14, 0x41);         // .

    /* 左边框 */
    at7456e_write_sram(4, 6, 0x41);          // .
    at7456e_write_sram(5, 6, 0x41);          // .
    at7456e_write_sram(6, 6, 0x41);          // .
    at7456e_write_sram(7, 6, 0x49);          // -
    at7456e_write_sram(8, 6, 0x41);          // .
    at7456e_write_sram(9, 6, 0x41);          // .
    at7456e_write_sram(10, 6, 0x41);         // .
    at7456e_write_sram(7, 7, 0x4B);          // >

    /* 右边框 */
    at7456e_write_sram(4, 22, 0x41);         // .
    at7456e_write_sram(5, 22, 0x41);         // .
    at7456e_write_sram(6, 22, 0x41);         // .
    at7456e_write_sram(7, 22, 0x49);         // -
    at7456e_write_sram(8, 22, 0x41);         // .
    at7456e_write_sram(9, 22, 0x41);         // .
    at7456e_write_sram(10, 22, 0x41);        // .
    at7456e_write_sram(7, 21, 0x4A);         // <

    /* Local measurements: height cm, battery V, BMP390 temperature C. */
    at7456e_write_sram(13,0,0x12); /* H */
    at7456e_write_sram(13,1,0x44);
    at7456e_write_sram(14,0,0x20); /* V */
    at7456e_write_sram(14,1,0x44);
    at7456e_write_sram(15,11,0x1E); /* T */
    at7456e_write_sram(15,12,0x44);
    at7456e_write_sram(15,18,0x0D); /* C */
    /* Calibration countdown in seconds. Master ARM is not on this link. */
    at7456e_write_sram(4,11,0x0D); /* C */
    at7456e_write_sram(4,12,0x0B); /* A */
    at7456e_write_sram(4,13,0x16); /* L */
    at7456e_write_sram(4,14,0x44);
    /* 打开 OSD 显示 */
    at7456e_osd_on();
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
static void display_integer(uint8_t row,uint8_t column,uint8_t width,int32_t number,uint8_t padded)
{
    uint8_t glyphs[10],i,first=(uint8_t)(number<0);
    uint32_t magnitude=number<0?0u-(uint32_t)number:(uint32_t)number;
    if(row>=16u || width==0u || width>10u || (unsigned)column+width>30u)return;
    for(i=0u;i<width;i++)glyphs[i]=padded?0x0Au:0u;
    if(first)glyphs[0]=0x49u;
    i=width;
    do {
        uint8_t digit=(uint8_t)(magnitude%10u);magnitude/=10u;
        if(i<=first)break;
        glyphs[--i]=digit?digit:0x0Au;
    } while(magnitude!=0u);
    for(i=0u;i<width;i++)at7456e_write_sram(row,column+i,glyphs[i]);
}
void osd_display_int_padded(uint8_t row,uint8_t column,uint8_t width,int32_t number)
{display_integer(row,column,width,number,1u);}
void osd_display_int(uint8_t row,uint8_t column,uint8_t width,int32_t number)
{display_integer(row,column,width,number,0u);}
void osd_display_float(uint8_t row,uint8_t column,uint8_t integer_digits,uint8_t fraction_digits,float number)
{
    uint8_t i;float fraction;
    if(!(number>=-1000000.0f && number<=1000000.0f) || row>=16u || integer_digits==0u ||
       integer_digits>7u || fraction_digits>4u || (unsigned)column+integer_digits+1u+fraction_digits>30u)return;
    osd_display_int(row,column,integer_digits,(int32_t)number);
    if(number<0 && number>-1)at7456e_write_sram(row,column,0x49u);
    at7456e_write_sram(row,column+integer_digits,0x41u);
    if(number<0)number=-number;
    fraction=number-(float)(uint32_t)number;
    for(i=0u;i<fraction_digits;i++) {
        uint8_t digit;fraction*=10.0f;digit=(uint8_t)fraction;
        if(digit>9u)digit=9u;
        at7456e_write_sram(row,column+integer_digits+1u+i,digit?digit:0x0Au);
        fraction-=(float)digit;
    }
}
