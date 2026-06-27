#include "stm32f10x.h"

/* 初始化 TIM1 更新中断。
 * 当前配置：72MHz / 7200 / 50 = 200Hz，即 5ms 触发一次。
 * 用途：周期性置位 CRSF 解析标志，实际解析放在主循环执行。
 */
void TIM1_Init_1S_IRQ(void)
{
	/* TIM1 挂在 APB2 总线上。 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
	
	TIM_InternalClockConfig(TIM1);
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	
	/* 72000000 / 7200 / 50 = 200Hz。 */
	TIM_TimeBaseInitStructure.TIM_Period = 50 - 1;
	TIM_TimeBaseInitStructure.TIM_Prescaler = 7200 - 1;
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);
	
	/* 清除更新标志，避免刚开启就进入一次中断。 */
	TIM_ClearFlag(TIM1, TIM_FLAG_Update);
	
	TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);
	
	/* NVIC 优先级低于核心姿态控制中断。 */
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2; 
	NVIC_InitStructure.NVIC_IRQChannelSubPriority =2;         
	NVIC_Init(&NVIC_InitStructure);
	
	TIM_Cmd(TIM1, ENABLE);
}
