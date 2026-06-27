#include "stm32f10x.h"
#include "Delay.h"

/* 软件 I2C 位操作延时。 */
#define I2C_DELAY_US 1

/* GPIO 时序控制。 */
void MyI2C_W_SCL(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_6, (BitAction)BitValue);
	Delay_us(I2C_DELAY_US);
}

void MyI2C_W_SDA(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_7, (BitAction)BitValue);
	Delay_us(I2C_DELAY_US);
}

uint8_t MyI2C_R_SDA(void)
{
	uint8_t BitValue;
	BitValue = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7);
	Delay_us(I2C_DELAY_US);
	return BitValue;
}

void MyI2C_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	GPIO_SetBits(GPIOA, GPIO_Pin_6 | GPIO_Pin_7);
}

/* I2C 协议时序。 */
void MyI2C_Start(void)
{
	MyI2C_W_SDA(1);
	MyI2C_W_SCL(1);
	MyI2C_W_SDA(0);
	MyI2C_W_SCL(0);
}

void MyI2C_Stop(void)
{
	MyI2C_W_SDA(0);
	MyI2C_W_SCL(1);
	MyI2C_W_SDA(1);
}

void MyI2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i ++)
	{
		MyI2C_W_SDA(Byte & (0x80 >> i));
		MyI2C_W_SCL(1);
		MyI2C_W_SCL(0);
	}
}

uint8_t MyI2C_ReceiveByte(void)
{
	uint8_t i, Byte = 0x00;
	MyI2C_W_SDA(1);
	for (i = 0; i < 8; i ++)
	{
		MyI2C_W_SCL(1);
		if (MyI2C_R_SDA() == 1){Byte |= (0x80 >> i);}
		MyI2C_W_SCL(0);
	}
	return Byte;
}

void MyI2C_SendAck(uint8_t AckBit)
{
	MyI2C_W_SDA(AckBit);
	MyI2C_W_SCL(1);
	MyI2C_W_SCL(0);
}

uint8_t MyI2C_ReceiveAck(void)
{
	uint8_t AckBit;
	MyI2C_W_SDA(1);
	MyI2C_W_SCL(1);
	AckBit = MyI2C_R_SDA();
	MyI2C_W_SCL(0);
	return AckBit;
}

/* 从指定寄存器开始连续读取多个字节。 */
void MyI2C_ReadBytes(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len)
{
	MyI2C_Start();
	MyI2C_SendByte(addr);          /* 设备地址：写 */
	MyI2C_ReceiveAck();
	MyI2C_SendByte(reg);           /* 起始寄存器地址 */
	MyI2C_ReceiveAck();
	
	MyI2C_Start();
	MyI2C_SendByte(addr | 0x01);   /* 设备地址：读 */
	MyI2C_ReceiveAck();
	
	for(uint8_t i=0; i<len; i++)
	{
		buf[i] = MyI2C_ReceiveByte();
		if(i < len-1) MyI2C_SendAck(0);  /* 前 n-1 个字节应答 */
		else MyI2C_SendAck(1);           /* 最后 1 个字节非应答 */
	}
	
	MyI2C_Stop();
}
