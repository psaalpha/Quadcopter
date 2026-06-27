#include "stm32f10x.h"

/* 初始化状态 LED：PC13、PA0、PA5，低电平点亮。 */
void LED_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC|RCC_APB2Periph_GPIOA, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_5;
		GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_SetBits(GPIOA, GPIO_Pin_0 | GPIO_Pin_5);
}

/* LED1：PC13。 */
void LED1_ON(void)
{
	GPIO_ResetBits(GPIOC, GPIO_Pin_13);
}

void LED1_OFF(void)
{
	GPIO_SetBits(GPIOC, GPIO_Pin_13);
}

/* LED2：PA0。 */
void LED2_ON(void)
{
	GPIO_ResetBits(GPIOA, GPIO_Pin_0);
}

void LED2_OFF(void)
{
	GPIO_SetBits(GPIOA, GPIO_Pin_0);
}

/* LED3：PA5。 */
void LED3_ON(void)
{
	GPIO_ResetBits(GPIOA, GPIO_Pin_5);
}

void LED3_OFF(void)
{
	GPIO_SetBits(GPIOA, GPIO_Pin_5);
}

