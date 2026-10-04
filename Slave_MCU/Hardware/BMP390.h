#ifndef BMP390_ADAPTER_H
#define BMP390_ADAPTER_H
#include "stm32f10x.h"
/* Bosch bmp3 SPI adapter. No second, unimplemented BMP390 API. */
void SPI1_Init(void);
int8_t bmp3_spi_read(uint8_t reg, uint8_t *data, uint32_t length, void *context);
int8_t bmp3_spi_write(uint8_t reg, const uint8_t *data, uint32_t length, void *context);
void BMP390_DelayUs(uint32_t period_us, void *context);
#endif
