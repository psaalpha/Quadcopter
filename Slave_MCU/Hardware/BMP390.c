#include "stm32f10x.h"
#include "bmp3.h"
#include <math.h>

/* BMP390 片选引脚：PA4。 */
#define BMP3_CS_LOW()   GPIO_ResetBits(GPIOA, GPIO_Pin_4)
#define BMP3_CS_HIGH()  GPIO_SetBits(GPIOA, GPIO_Pin_4)

/* 初始化 SPI1，用于 BMP390。 */
void SPI1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    SPI_InitTypeDef SPI_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    /* PA4：软件片选，默认拉高。 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_4);

    /* PA5/PA7：SCK/MOSI。 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA6：MISO。 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_Init(SPI1, &SPI_InitStructure);
    SPI_Cmd(SPI1, ENABLE);
}

/* SPI1 收发一个字节。 */
uint8_t SPI_ReadWrite(uint8_t tx_data)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI1, tx_data);
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
    return SPI_I2S_ReceiveData(SPI1);
}

/* Bosch bmp3 驱动适配：SPI 读寄存器。 */
int8_t bmp3_spi_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    BMP3_CS_LOW();
    SPI_ReadWrite(reg_addr | 0x80);
    /* bmp3_get_regs() 内部会处理 SPI dummy 字节，
     * 此处只需按 len 读取。
     */
    for (uint32_t i = 0; i < len; i++) {
        reg_data[i] = SPI_ReadWrite(0xFF);
    }
    BMP3_CS_HIGH();
    return 0;
}

/* Bosch bmp3 驱动适配：SPI 写寄存器。 */
int8_t bmp3_spi_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    BMP3_CS_LOW();
    SPI_ReadWrite(reg_addr & 0x7F);
    for (uint32_t i = 0; i < len; i++) {
        SPI_ReadWrite(reg_data[i]);
    }
    BMP3_CS_HIGH();
    return 0;
}

