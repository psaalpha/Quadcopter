#ifndef SLAVE_MCU_DRIVERS_BUS_SOFT_SPI_H
#define SLAVE_MCU_DRIVERS_BUS_SOFT_SPI_H

#include <stdint.h>

void soft_spi_init(void);
void soft_spi_start(void);
void soft_spi_stop(void);
uint8_t soft_spi_transfer(uint8_t tx_byte);

#endif
