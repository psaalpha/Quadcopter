#include "stm32f10x.h"

/* 初始化状态 LED：PC13、PA0、PA5，低电平点亮。 */
void board_led_init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC|RCC_APB2Periph_GPIOA, ENABLE);

	GPIO_InitTypeDef gpio_config;
	gpio_config.GPIO_Mode = GPIO_Mode_Out_PP;
	gpio_config.GPIO_Pin = GPIO_Pin_13;
	gpio_config.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &gpio_config);
	gpio_config.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_5;
		GPIO_Init(GPIOA, &gpio_config);

	GPIO_SetBits(GPIOA, GPIO_Pin_0 | GPIO_Pin_5);
}

/* LED1：PC13。 */
void board_led1_on(void)
{
	GPIO_ResetBits(GPIOC, GPIO_Pin_13);
}

void board_led1_off(void)
{
	GPIO_SetBits(GPIOC, GPIO_Pin_13);
}

/* LED2：PA0。 */
void board_led2_on(void)
{
	GPIO_ResetBits(GPIOA, GPIO_Pin_0);
}

void board_led2_off(void)
{
	GPIO_SetBits(GPIOA, GPIO_Pin_0);
}

/* LED3：PA5。 */
void board_led3_on(void)
{
	GPIO_ResetBits(GPIOA, GPIO_Pin_5);
}

void board_led3_off(void)
{
	GPIO_SetBits(GPIOA, GPIO_Pin_5);
}
