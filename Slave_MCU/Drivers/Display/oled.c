#include "stm32f10x.h"
#include "oled_font.h"
#include <math.h>
/*引脚配置*/
#define OLED_W_SCL(x)		GPIO_WriteBit(GPIOB, GPIO_Pin_8, (BitAction)(x))
#define OLED_W_SDA(x)		GPIO_WriteBit(GPIOB, GPIO_Pin_9, (BitAction)(x))

/*引脚初始化*/
void oled_i2c_init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitTypeDef gpio_config;
	gpio_config.GPIO_Mode = GPIO_Mode_Out_OD;
	gpio_config.GPIO_Speed = GPIO_Speed_50MHz;
	gpio_config.GPIO_Pin = GPIO_Pin_8;
	GPIO_Init(GPIOB, &gpio_config);
	gpio_config.GPIO_Pin = GPIO_Pin_9;
	GPIO_Init(GPIOB, &gpio_config);

	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
  * @brief  I2C开始
  * @param  无
  * @retval 无
  */
void oled_i2c_start(void)
{
	OLED_W_SDA(1);
	OLED_W_SCL(1);
	OLED_W_SDA(0);
	OLED_W_SCL(0);
}

/**
  * @brief  I2C停止
  * @param  无
  * @retval 无
  */
void oled_i2c_stop(void)
{
	OLED_W_SDA(0);
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
  * @brief  I2C发送一个字节
  * @param  byte 要发送的一个字节
  * @retval 无
  */
void oled_i2c_send_byte(uint8_t byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		OLED_W_SDA(byte & (0x80 >> i));
		OLED_W_SCL(1);
		OLED_W_SCL(0);
	}
	OLED_W_SCL(1);	//额外的一个时钟，不处理应答信号
	OLED_W_SCL(0);
}

/**
  * @brief  OLED写命令
  * @param  command 要写入的命令
  * @retval 无
  */
void oled_write_command(uint8_t command)
{
	oled_i2c_start();
	oled_i2c_send_byte(0x78);		//从机地址
	oled_i2c_send_byte(0x00);		//写命令
	oled_i2c_send_byte(command);
	oled_i2c_stop();
}

/**
  * @brief  OLED写数据
  * @param  data 要写入的数据
  * @retval 无
  */
void oled_write_data(uint8_t data)
{
	oled_i2c_start();
	oled_i2c_send_byte(0x78);		//从机地址
	oled_i2c_send_byte(0x40);		//写数据
	oled_i2c_send_byte(data);
	oled_i2c_stop();
}

/**
  * @brief  OLED设置光标位置
  * @param  exponent 以左上角为原点，向下方向的坐标，范围：0~7
  * @param  base 以左上角为原点，向右方向的坐标，范围：0~127
  * @retval 无
  */
void oled_set_cursor(uint8_t exponent, uint8_t base)
{
	oled_write_command(0xB0 | exponent);					//设置Y位置
	oled_write_command(0x10 | ((base & 0xF0) >> 4));	//设置X位置高4位
	oled_write_command(0x00 | (base & 0x0F));			//设置X位置低4位
}

/**
  * @brief  OLED清屏
  * @param  无
  * @retval 无
  */
void oled_clear(void)
{
	uint8_t i, j;
	for (j = 0; j < 8; j++)
	{
		oled_set_cursor(j, 0);
		for(i = 0; i < 128; i++)
		{
			oled_write_data(0x00);
		}
	}
}

/**
  * @brief  OLED显示一个字符
  * @param  line 行位置，范围：1~4
  * @param  column 列位置，范围：1~16
  * @param  character 要显示的一个字符，范围：ASCII可见字符
  * @retval 无
  */
void oled_show_char(uint8_t line, uint8_t column, char character)
{
	uint8_t i;
	oled_set_cursor((line - 1) * 2, (column - 1) * 8);		//设置光标位置在上半部分
	for (i = 0; i < 8; i++)
	{
		oled_write_data(OLED_F8x16[character - ' '][i]);			//显示上半部分内容
	}
	oled_set_cursor((line - 1) * 2 + 1, (column - 1) * 8);	//设置光标位置在下半部分
	for (i = 0; i < 8; i++)
	{
		oled_write_data(OLED_F8x16[character - ' '][i + 8]);		//显示下半部分内容
	}
}

/**
  * @brief  OLED显示字符串
  * @param  line 起始行位置，范围：1~4
  * @param  column 起始列位置，范围：1~16
  * @param  string 要显示的字符串，范围：ASCII可见字符
  * @retval 无
  */
void oled_show_string(uint8_t line, uint8_t column, char *string)
{
	uint8_t i;
	for (i = 0; string[i] != '\0'; i++)
	{
		oled_show_char(line, column + i, string[i]);
	}
}

/**
  * @brief  OLED次方函数
  * @retval 返回值等于X的Y次方
  */
uint32_t oled_pow(uint32_t base, uint32_t exponent)
{
	uint32_t result = 1;
	while (exponent--)
	{
		result *= base;
	}
	return result;
}

/**
  * @brief  OLED显示数字（十进制，正数）
  * @param  line 起始行位置，范围：1~4
  * @param  column 起始列位置，范围：1~16
  * @param  number 要显示的数字，范围：0~4294967295
  * @param  length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void oled_show_num(uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
	uint8_t i;
	for (i = 0; i < length; i++)
	{
		oled_show_char(line, column + i, number / oled_pow(10, length - i - 1) % 10 + '0');
	}
}

/**
  * @brief  OLED显示数字（十进制，带符号数）
  * @param  line 起始行位置，范围：1~4
  * @param  column 起始列位置，范围：1~16
  * @param  number 要显示的数字，范围：-2147483648~2147483647
  * @param  length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void oled_show_signed_num(uint8_t line, uint8_t column, int32_t number, uint8_t length)
{
	uint8_t i;
	uint32_t magnitude;
	if (number >= 0)
	{
		oled_show_char(line, column, '+');
		magnitude = number;
	}
	else
	{
		oled_show_char(line, column, '-');
		magnitude = -number;
	}
	for (i = 0; i < length; i++)
	{
		oled_show_char(line, column + i + 1, magnitude / oled_pow(10, length - i - 1) % 10 + '0');
	}
}

/**
  * @brief  OLED显示数字（十六进制，正数）
  * @param  line 起始行位置，范围：1~4
  * @param  column 起始列位置，范围：1~16
  * @param  number 要显示的数字，范围：0~0xFFFFFFFF
  * @param  length 要显示数字的长度，范围：1~8
  * @retval 无
  */
void oled_show_hex_num(uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < length; i++)
	{
		SingleNumber = number / oled_pow(16, length - i - 1) % 16;
		if (SingleNumber < 10)
		{
			oled_show_char(line, column + i, SingleNumber + '0');
		}
		else
		{
			oled_show_char(line, column + i, SingleNumber - 10 + 'A');
		}
	}
}

/**
  * @brief  OLED显示数字（二进制，正数）
  * @param  line 起始行位置，范围：1~4
  * @param  column 起始列位置，范围：1~16
  * @param  number 要显示的数字，范围：0~1111 1111 1111 1111
  * @param  length 要显示数字的长度，范围：1~16
  * @retval 无
  */
void oled_show_bin_num(uint8_t line, uint8_t column, uint32_t number, uint8_t length)
{
	uint8_t i;
	for (i = 0; i < length; i++)
	{
		oled_show_char(line, column + i, number / oled_pow(2, length - i - 1) % 2 + '0');
	}
}

/**
  * @brief  OLED初始化
  * @param  无
  * @retval 无
  */
void oled_init(void)
{
	uint32_t i, j;

	for (i = 0; i < 1000; i++)			//上电延时
	{
		for (j = 0; j < 1000; j++);
	}

	oled_i2c_init();			//端口初始化

	oled_write_command(0xAE);	//关闭显示

	oled_write_command(0xD5);	//设置显示时钟分频比/振荡器频率
	oled_write_command(0x80);

	oled_write_command(0xA8);	//设置多路复用率
	oled_write_command(0x3F);

	oled_write_command(0xD3);	//设置显示偏移
	oled_write_command(0x00);

	oled_write_command(0x40);	//设置显示开始行

	oled_write_command(0xA1);	//设置左右方向，0xA1正常 0xA0左右反置

	oled_write_command(0xC8);	//设置上下方向，0xC8正常 0xC0上下反置

	oled_write_command(0xDA);	//设置COM引脚硬件配置
	oled_write_command(0x12);

	oled_write_command(0x81);	//设置对比度控制
	oled_write_command(0xCF);

	oled_write_command(0xD9);	//设置预充电周期
	oled_write_command(0xF1);

	oled_write_command(0xDB);	//设置VCOMH取消选择级别
	oled_write_command(0x30);

	oled_write_command(0xA4);	//设置整个显示打开/关闭

	oled_write_command(0xA6);	//设置正常/倒转显示

	oled_write_command(0x8D);	//设置充电泵
	oled_write_command(0x14);

	oled_write_command(0xAF);	//开启显示

	oled_clear();				//OLED清屏
}

/**
 * @brief  OLED显示浮点数（修复-0.XX类数值显示问题，适配正负浮点数）
 * @param  line：OLED显示行号（1-4）
 * @param  column：显示起始列号（1-16）
 * @param  value：要显示的浮点数（支持正负，例：0.66 / -0.66 / 1.66 / -1.66）
 * @param  integer_len：整数部分显示位数（包含负号，例：传3表示整数部分占3列）
 * @note   固定显示2位小数，仅修改负数0的显示逻辑，最小改动
 */
void oled_show_float(uint8_t line, uint8_t column, float value, uint8_t integer_len)
{
    // 1. 标记数值是否为负数（核心修复：单独处理负号，避免-0被吞）
    uint8_t is_negative = 0; // 0=正数/0，1=负数
    if (value < 0)
    {
        is_negative = 1;     // 标记为负数
        value = -value;         // 取绝对值，统一后续计算逻辑
    }

    // 2. 提取整数部分：四舍五入取整（原逻辑保留，仅处理绝对值）
    // 例：0.66→1？不，0.66+0.005=0.665→取整为0；1.66→1.665→取整为1
    int integer_part = (int)(value + 0.005);

    // 3. 提取两位小数部分：乘以100取整后取余，仅保留后两位
    // 例：0.66→0.66*100=66→66%100=66；1.66→166%100=66
    float abs_num = value; // 已通过上面逻辑转为绝对值，直接使用
    int decimal_part = (int)(abs_num * 100) % 100;

    // 4. 核心修复1：单独处理负数标记，先显示负号（避免-0被吞）
    if (is_negative)
    {
        oled_show_char(line, column, '-'); // 负数先显示负号
        column++; // 列号后移1位，给负号留位置
    }
	else
	{
		oled_show_char(line, column, '+');
		        column++;

	}

    // 5. 显示整数部分（仅显示绝对值，负号已单独处理）
    // 例：-0.66→integer_part=0，此处显示0；-1.66→integer_part=1，此处显示1
    oled_show_num(line, column, integer_part, integer_len);

    // 6. 核心修复2：小数点位置计算（起始列+整数部分显示位数）
    // 例：column=8，integer_len=1 → 小数点在8+1=9列（整数占8列，小数点占9列）
    uint8_t dot_column = column + integer_len;

    // 7. 显示小数点（在整数部分后一列，避免覆盖）
    oled_show_char(line, dot_column, '.');

    // 8. 显示两位小数（小数点后一列开始，不足两位自动补0）
    // 例：0.66→显示66；0.5→显示50；1.66→显示66
    oled_show_num(line, dot_column + 1, decimal_part, 2);
}
