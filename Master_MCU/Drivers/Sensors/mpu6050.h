#ifndef MASTER_MCU_DRIVERS_SENSORS_MPU6050_H
#define MASTER_MCU_DRIVERS_SENSORS_MPU6050_H

#include <stdint.h>

void mpu6050_write_reg(uint8_t register_address, uint8_t data);
uint8_t mpu6050_read_reg(uint8_t register_address);
void mpu6050_read_regs(uint8_t register_address, uint8_t *data, uint8_t length);
void mpu6050_init(void);
uint8_t mpu6050_get_id(void);
void mpu6050_get_data(int16_t *accel_x, int16_t *accel_y, int16_t *accel_z,
						int16_t *gyro_x, int16_t *gyro_y, int16_t *gyro_z);

#endif
