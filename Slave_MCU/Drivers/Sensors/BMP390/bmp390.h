#ifndef SLAVE_MCU_DRIVERS_SENSORS_BMP390_BMP390_H
#define SLAVE_MCU_DRIVERS_SENSORS_BMP390_BMP390_H
#include "stm32f10x.h"
/* Bosch bmp3 SPI adapter. No second, unimplemented BMP390 API. */
void bmp390_spi_init(void);
int8_t bmp390_spi_read(uint8_t reg, uint8_t *data, uint32_t length, void *context);
int8_t bmp390_spi_write(uint8_t reg, const uint8_t *data, uint32_t length, void *context);
void bmp390_delay_us(uint32_t period_us, void *context);
#endif
