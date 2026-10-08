#include "stm32f10x.h"
#include "soft_i2c.h"
#include "mpu6050_regs.h"

#define MPU6050_ADDRESS		0xD0		/* MPU6050 I2C 写地址 */

/* 写 MPU6050 单个寄存器。 */
void mpu6050_write_reg(uint8_t register_address, uint8_t data)
{
	soft_i2c_start();
	soft_i2c_send_byte(MPU6050_ADDRESS);
	soft_i2c_receive_ack();
	soft_i2c_send_byte(register_address);
	soft_i2c_receive_ack();
	soft_i2c_send_byte(data);
	soft_i2c_receive_ack();
	soft_i2c_stop();
}

/* 读取 MPU6050 单个寄存器。 */
uint8_t mpu6050_read_reg(uint8_t register_address)
{
	uint8_t data;

	soft_i2c_start();
	soft_i2c_send_byte(MPU6050_ADDRESS);
	soft_i2c_receive_ack();
	soft_i2c_send_byte(register_address);
	soft_i2c_receive_ack();

	soft_i2c_start();
	soft_i2c_send_byte(MPU6050_ADDRESS | 0x01);
	soft_i2c_receive_ack();
	data = soft_i2c_receive_byte();
	soft_i2c_send_ack(1);
	soft_i2c_stop();

	return data;
}

/* 从指定起始寄存器连续读取多个字节。 */
void mpu6050_read_regs(uint8_t register_address, uint8_t *data, uint8_t length)
{
	soft_i2c_start();
	soft_i2c_send_byte(MPU6050_ADDRESS);
	soft_i2c_receive_ack();
	soft_i2c_send_byte(register_address);
	soft_i2c_receive_ack();

	soft_i2c_start();
	soft_i2c_send_byte(MPU6050_ADDRESS | 0x01);
	soft_i2c_receive_ack();

	for(uint8_t i=0; i<length; i++)
	{
		data[i] = soft_i2c_receive_byte();
		if(i < length-1)
		{
			soft_i2c_send_ack(0);
		}
		else
		{
			soft_i2c_send_ack(1);
		}
	}

	soft_i2c_stop();
}

/* 初始化 MPU6050：陀螺仪输出 8kHz、加速度计更新 1kHz；主循环读取 500Hz。
 * 量程：陀螺仪 ±2000dps、加速度计 ±16g。 */
void mpu6050_init(void)
{
	soft_i2c_init();

	mpu6050_write_reg(MPU6050_PWR_MGMT_1, 0x01);
	mpu6050_write_reg(MPU6050_PWR_MGMT_2, 0x00);
	mpu6050_write_reg(MPU6050_SMPLRT_DIV, 0x00);
	mpu6050_write_reg(MPU6050_CONFIG, 0x00);
	mpu6050_write_reg(MPU6050_GYRO_CONFIG, 0x18);
	mpu6050_write_reg(MPU6050_ACCEL_CONFIG, 0x18);
}

/* 读取 MPU6050 WHO_AM_I。 */
uint8_t mpu6050_get_id(void)
{
	return mpu6050_read_reg(MPU6050_WHO_AM_I);
}

/* 批量读取加速度和陀螺仪数据，一次 I2C 事务读取 14 字节。 */
void mpu6050_get_data(int16_t *accel_x, int16_t *accel_y, int16_t *accel_z,
						int16_t *gyro_x, int16_t *gyro_y, int16_t *gyro_z)
{
	uint8_t data[14];	/* 加速度(6)+温度(2)+陀螺仪(6) */

	mpu6050_read_regs(MPU6050_ACCEL_XOUT_H, data, 14);

	*accel_x = (data[0] << 8) | data[1];
	*accel_y = (data[2] << 8) | data[3];
	*accel_z = (data[4] << 8) | data[5];
	*accel_z = -(*accel_z);

	*gyro_x = (data[8] << 8) | data[9];
	*gyro_y = (data[10] << 8) | data[11];
	*gyro_z = (data[12] << 8) | data[13];
	*gyro_z = -(*gyro_z);
}
