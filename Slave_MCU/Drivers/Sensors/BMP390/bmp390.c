#include "stm32f10x.h"
#include "bmp3.h"
#include "bmp390.h"
#include "Delay.h"
#include <math.h>

/* BMP390 片选引脚：PA4。 */
#define BMP3_CS_LOW()   GPIO_ResetBits(GPIOA, GPIO_Pin_4)
#define BMP3_CS_HIGH()  GPIO_SetBits(GPIOA, GPIO_Pin_4)

/* 初始化 SPI1，用于 BMP390。 */
void bmp390_spi_init(void)
{
    GPIO_InitTypeDef gpio_config;
    SPI_InitTypeDef SPI_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

    /* PA4：软件片选，默认拉高。 */
    gpio_config.GPIO_Pin = GPIO_Pin_4;
    gpio_config.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_config.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio_config);
    GPIO_SetBits(GPIOA, GPIO_Pin_4);

    /* PA5/PA7：SCK/MOSI。 */
    gpio_config.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    gpio_config.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio_config);

    /* PA6：MISO。 */
    gpio_config.GPIO_Pin = GPIO_Pin_6;
    gpio_config.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio_config);

    SPI_StructInit(&SPI_InitStructure);
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

static uint8_t spi_error;
/* SPI1 收发一个字节；有限等待避免外设故障卡死主循环。 */
uint8_t bmp390_spi_transfer(uint8_t tx_data)
{
    uint32_t timeout = 2000u;
    if (spi_error != 0u) return 0xFFu;
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET && timeout != 0u) timeout--;
    if (timeout == 0u) { spi_error = 1u; return 0xFFu; }
    SPI_I2S_SendData(SPI1, tx_data);
    timeout = 2000u;
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET && timeout != 0u) timeout--;
    if (timeout == 0u) { spi_error = 1u; return 0xFFu; }
    return SPI_I2S_ReceiveData(SPI1);
}

/* Bosch bmp3 驱动适配：SPI 读寄存器。 */
int8_t bmp390_spi_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    (void)intf_ptr;
    spi_error = 0u;
    BMP3_CS_LOW();
    bmp390_spi_transfer(reg_addr | 0x80);
    /* bmp3_get_regs() 内部会处理 SPI dummy 字节，
     * 此处只需按 len 读取。
     */
    for (uint32_t i = 0; i < len; i++) {
        reg_data[i] = bmp390_spi_transfer(0xFF);
    }
    BMP3_CS_HIGH();
    return spi_error != 0u ? -1 : 0;
}

/* Bosch bmp3 驱动适配：SPI 写寄存器。 */
int8_t bmp390_spi_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    (void)intf_ptr;
    spi_error = 0u;
    BMP3_CS_LOW();
    bmp390_spi_transfer(reg_addr & 0x7F);
    for (uint32_t i = 0; i < len; i++) {
        bmp390_spi_transfer(reg_data[i]);
    }
    BMP3_CS_HIGH();
    return spi_error != 0u ? -1 : 0;
}


void bmp390_delay_us(uint32_t period_us, void *context)
{
    (void)context;
    while (period_us >= 1000u) {
        Delay_us(1000u);
        period_us -= 1000u;
    }
    if (period_us != 0u) Delay_us(period_us);
}
