#include "stm32f10x.h"                  // Device header

/*引脚配置层*/

/**
  * 函    数：SPI写SS引脚电平
  * 参    数：level 协议层传入的当前需要写入SS的电平，范围0~1
  * 返 回 值：无
  * 注意事项：此函数需要用户实现内容，当BitValue为0时，需要置SS为低电平，当BitValue为1时，需要置SS为高电平
  */
void soft_spi_write_ss(uint8_t level)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_4, (BitAction)level);		//根据BitValue，设置SS引脚的电平
}

/**
  * 函    数：SPI写SCK引脚电平
  * 参    数：level 协议层传入的当前需要写入SCK的电平，范围0~1
  * 返 回 值：无
  * 注意事项：此函数需要用户实现内容，当BitValue为0时，需要置SCK为低电平，当BitValue为1时，需要置SCK为高电平
  */
void soft_spi_write_sck(uint8_t level)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_5, (BitAction)level);		//根据BitValue，设置SCK引脚的电平
}

/**
  * 函    数：SPI写MOSI引脚电平
  * 参    数：level 协议层传入的当前需要写入MOSI的电平，范围0~0xFF
  * 返 回 值：无
  * 注意事项：此函数需要用户实现内容，当BitValue为0时，需要置MOSI为低电平，当BitValue非0时，需要置MOSI为高电平
  */
void soft_spi_write_mosi(uint8_t level)
{
	GPIO_WriteBit(GPIOA, GPIO_Pin_7, (BitAction)level);		//根据BitValue，设置MOSI引脚的电平，BitValue要实现非0即1的特性
}

/**
  * 函    数：I2C读MISO引脚电平
  * 参    数：无
  * 返 回 值：协议层需要得到的当前MISO的电平，范围0~1
  * 注意事项：此函数需要用户实现内容，当前MISO为低电平时，返回0，当前MISO为高电平时，返回1
  */
uint8_t soft_spi_read_miso(void)
{
	return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6);			//读取MISO电平并返回
}

/**
  * 函    数：SPI初始化
  * 参    数：无
  * 返 回 值：无
  * 注意事项：此函数需要用户实现内容，实现SS、SCK、MOSI和MISO引脚的初始化
  */
void soft_spi_init(void)
{
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	//开启GPIOA的时钟

	/*GPIO初始化*/
	GPIO_InitTypeDef gpio_config;
	gpio_config.GPIO_Mode = GPIO_Mode_Out_PP;
	gpio_config.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_7;
	gpio_config.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio_config);					//将PA4、PA5和PA7引脚初始化为推挽输出

	gpio_config.GPIO_Mode = GPIO_Mode_IPU;
	gpio_config.GPIO_Pin = GPIO_Pin_6;
	gpio_config.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio_config);					//将PA6引脚初始化为上拉输入

	/*设置默认电平*/
	soft_spi_write_ss(1);											//SS默认高电平
	soft_spi_write_sck(0);											//SCK默认低电平
}

/*协议层*/

/**
  * 函    数：SPI起始
  * 参    数：无
  * 返 回 值：无
  */
void soft_spi_start(void)
{
	soft_spi_write_ss(0);				//拉低SS，开始时序
}

/**
  * 函    数：SPI终止
  * 参    数：无
  * 返 回 值：无
  */
void soft_spi_stop(void)
{
	soft_spi_write_ss(1);				//拉高SS，终止时序
}

/**
  * 函    数：SPI交换传输一个字节，使用SPI模式0
  * 参    数：tx_byte 要发送的一个字节
  * 返 回 值：接收的一个字节
  */
uint8_t soft_spi_transfer(uint8_t tx_byte)
{
	uint8_t i, rx_byte = 0x00;					//定义接收的数据，并赋初值0x00，此处必须赋初值0x00，后面会用到

	for (i = 0; i < 8; i ++)						//循环8次，依次交换每一位数据
	{
		soft_spi_write_mosi(tx_byte & (0x80 >> i));		//使用掩码的方式取出ByteSend的指定一位数据并写入到MOSI线
		soft_spi_write_sck(1);								//拉高SCK，上升沿移出数据
		if (soft_spi_read_miso() == 1){rx_byte |= (0x80 >> i);}	//读取MISO数据，并存储到Byte变量
																//当MISO为1时，置变量指定位为1，当MISO为0时，不做处理，指定位为默认的初值0
		soft_spi_write_sck(0);								//拉低SCK，下降沿移入数据
	}

	return rx_byte;								//返回接收到的一个字节数据
}
