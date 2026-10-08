#ifndef MASTER_MCU_DRIVERS_BUS_SOFT_I2C_H
#define MASTER_MCU_DRIVERS_BUS_SOFT_I2C_H

#include <stdint.h>


/*引脚配置层*/
void soft_i2c_write_scl(uint8_t level);
void soft_i2c_write_sda(uint8_t level);
uint8_t soft_i2c_read_sda(void);
void soft_i2c_init(void);
void soft_i2c_start(void);
void soft_i2c_stop(void);
void soft_i2c_send_byte(uint8_t byte);
uint8_t soft_i2c_receive_byte(void);

void soft_i2c_send_ack(uint8_t ack_bit);

uint8_t soft_i2c_receive_ack(void);
void soft_i2c_read_bytes(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len);

#endif
