#include "stm32f10x.h"
#include "Delay.h"

/* 软件 I2C 位操作延时。 */
#define I2C_DELAY_US 1

/* GPIO 时序控制。 */
void soft_i2c_write_scl(uint8_t level)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_6, (BitAction)level);
	Delay_us(I2C_DELAY_US);
}

void soft_i2c_write_sda(uint8_t level)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_7, (BitAction)level);
	Delay_us(I2C_DELAY_US);
}

uint8_t soft_i2c_read_sda(void)
{
	uint8_t level;
	level = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7);
	Delay_us(I2C_DELAY_US);
	return level;
}

void soft_i2c_init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	GPIO_InitTypeDef gpio_config;
	gpio_config.GPIO_Mode = GPIO_Mode_Out_OD;
	gpio_config.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	gpio_config.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio_config);

	GPIO_SetBits(GPIOA, GPIO_Pin_6 | GPIO_Pin_7);
}

/* I2C 协议时序。 */
void soft_i2c_start(void)
{
	soft_i2c_write_sda(1);
	soft_i2c_write_scl(1);
	soft_i2c_write_sda(0);
	soft_i2c_write_scl(0);
}

void soft_i2c_stop(void)
{
	soft_i2c_write_sda(0);
	soft_i2c_write_scl(1);
	soft_i2c_write_sda(1);
}

void soft_i2c_send_byte(uint8_t byte)
{
	uint8_t i;
	for (i = 0; i < 8; i ++)
	{
		soft_i2c_write_sda(byte & (0x80 >> i));
		soft_i2c_write_scl(1);
		soft_i2c_write_scl(0);
	}
}

uint8_t soft_i2c_receive_byte(void)
{
	uint8_t i, byte = 0x00;
	soft_i2c_write_sda(1);
	for (i = 0; i < 8; i ++)
	{
		soft_i2c_write_scl(1);
		if (soft_i2c_read_sda() == 1){byte |= (0x80 >> i);}
		soft_i2c_write_scl(0);
	}
	return byte;
}

void soft_i2c_send_ack(uint8_t ack_bit)
{
	soft_i2c_write_sda(ack_bit);
	soft_i2c_write_scl(1);
	soft_i2c_write_scl(0);
}

uint8_t soft_i2c_receive_ack(void)
{
	uint8_t ack_bit;
	soft_i2c_write_sda(1);
	soft_i2c_write_scl(1);
	ack_bit = soft_i2c_read_sda();
	soft_i2c_write_scl(0);
	return ack_bit;
}

/* 从指定寄存器开始连续读取多个字节。 */
void soft_i2c_read_bytes(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len)
{
	soft_i2c_start();
	soft_i2c_send_byte(addr);          /* 设备地址：写 */
	soft_i2c_receive_ack();
	soft_i2c_send_byte(reg);           /* 起始寄存器地址 */
	soft_i2c_receive_ack();

	soft_i2c_start();
	soft_i2c_send_byte(addr | 0x01);   /* 设备地址：读 */
	soft_i2c_receive_ack();

	for(uint8_t i=0; i<len; i++)
	{
		buf[i] = soft_i2c_receive_byte();
		if(i < len-1) soft_i2c_send_ack(0);  /* 前 n-1 个字节应答 */
		else soft_i2c_send_ack(1);           /* 最后 1 个字节非应答 */
	}

	soft_i2c_stop();
}
