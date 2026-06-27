#include "stm32f10x.h"
#include "bmp3.h"
#include <math.h>   // 用于海拔计算

// ---------- 定义CS引脚 ----------
#define BMP3_CS_LOW()   GPIO_ResetBits(GPIOA, GPIO_Pin_4)
#define BMP3_CS_HIGH()  GPIO_SetBits(GPIOA, GPIO_Pin_4)

// ---------- SPI1 初始化（你的板子已经写好了，但这里确保调用一次）----------
void SPI1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef SPI_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    // CS - PA4 推挽输出，默认高
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_4);

    // SCK, MOSI - PA5, PA7 复用推挽
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // MISO - PA6 浮空输入（或上拉输入）
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;      // 时钟空闲低
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;    // 第一个边沿采样
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;  // 9MHz（72M/8=9M，小于10M）
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_Init(SPI1, &SPI_InitStructure);
    SPI_Cmd(SPI1, ENABLE);
}

// ---------- SPI 收发一个字节 ----------
uint8_t SPI_ReadWrite(uint8_t tx_data)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI1, tx_data);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(SPI1);
}

// ---------- 提供给官方驱动的读函数 ----------
int8_t bmp3_spi_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    BMP3_CS_LOW();
    // 发送读命令（寄存器地址最高位为1）
    SPI_ReadWrite(reg_addr | 0x80);
    /* 注意：官方库 bmp3_get_regs() 内部已经处理了 SPI dummy 字节
     * (dev->dummy_byte = 1)，会多读1个字节并跳过第1个。
     * 所以此函数只需按 len 读取即可，无需额外加 dummy。
     */
    for (uint32_t i = 0; i < len; i++) {
        reg_data[i] = SPI_ReadWrite(0xFF);
    }
    BMP3_CS_HIGH();
    return 0;   // 0表示成功
}

// ---------- 提供给官方驱动的写函数 ----------
int8_t bmp3_spi_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    BMP3_CS_LOW();
    // 发送写命令（寄存器地址最高位为0）
    SPI_ReadWrite(reg_addr & 0x7F);
    for (uint32_t i = 0; i < len; i++) {
        SPI_ReadWrite(reg_data[i]);
    }
    BMP3_CS_HIGH();
    return 0;
}

