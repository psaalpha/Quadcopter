#include "stm32f10x.h"                  // Device header

/**
  * 函    数：TIM1 1秒中断初始化
  * 优    先 级：抢占3，响应1
  * 时钟：72MHz
  */
void TIM1_Init_1S_IRQ(void)
{
	/* 开启时钟：TIM1 挂在 APB2 上 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
	
	/* 选择内部时钟 */
	TIM_InternalClockConfig(TIM1);
	
	/* 时基配置：实现 1 秒中断 */
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	
	// 计算公式：(PSC+1)*(ARR+1) / 72M = 1秒
	// 7200 * 10000 / 72000000 = 1s
	TIM_TimeBaseInitStructure.TIM_Period = 50 - 1;    // ARR
	TIM_TimeBaseInitStructure.TIM_Prescaler = 7200 - 1;  // PSC
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0; // 高级定时器必须写0
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);
	
	/* 清除更新标志，避免刚开启就进中断 */
	TIM_ClearFlag(TIM1, TIM_FLAG_Update);
	
	/* 开启更新中断 */
	TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);
	
//	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	
	/* NVIC 配置：抢占3，响应1 */
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_IRQn;  // TIM1 更新中断通道
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2; 
	NVIC_InitStructure.NVIC_IRQChannelSubPriority =2;         
	NVIC_Init(&NVIC_InitStructure);
	
	/* 启动定时器 */
	TIM_Cmd(TIM1, ENABLE);
}

/**
  * 函    数：TIM1 更新中断服务函数
  * 功    能：每 1 秒进入一次
  */
//void TIM1_UP_IRQHandler(void)
//{
//	// 判断是否为更新中断
//	if (TIM_GetITStatus(TIM1, TIM_IT_Update) == SET)
//	{
//		// ==============================
//		// 在这里写 1秒 执行一次的代码
//		// ==============================
//		
//		// 清除中断标志位
//		TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
//	}
//}
