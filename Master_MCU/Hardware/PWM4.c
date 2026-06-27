#include "stm32f10x.h"                  // Device header

/**
  * 函    数：TIM4 四路 PWM 初始化（无刷电调 ESC）
  * 通    道：CH1-PB6、CH2-PB7、CH3-PB8、CH4-PB9
  * 频    率：50Hz（72MHz / 144 / 10000 = 50Hz, 20ms 周期）
  * 脉    宽：500~1000 对应 1000~2000μs
  */
void PWM4_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	TIM_InternalClockConfig(TIM4);
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period = 10000 - 1;           //ARR 20ms 500kHz
	TIM_TimeBaseInitStructure.TIM_Prescaler = 144 - 1;          // 72MHz / 144 = 500kHz
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseInitStructure);
	
	TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  // 分组2：2位抢占，2位子优先级
	NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;  // 抢占优先级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;         // 子优先级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 500;                        // 初始 1000μs（电机停转）
	
	TIM_OC1Init(TIM4, &TIM_OCInitStructure);    // CH1: PB6
	TIM_OC2Init(TIM4, &TIM_OCInitStructure);    // CH2: PB7
	TIM_OC3Init(TIM4, &TIM_OCInitStructure);    // CH3: PB8
	TIM_OC4Init(TIM4, &TIM_OCInitStructure);    // CH4: PB9
	
	TIM_Cmd(TIM4, ENABLE);
}

/**
  * 函    数：设置 TIM4_CH1 占空比（ESC 脉宽）
  * 参    数：Compare：500~1000（1000~2000μs）
  *           500/10000=1ms;1000/10000=2ms
  */
void PWM4_SetCompare1(uint16_t Compare)
{
	if(Compare > 1000) Compare = 1000;
	if(Compare <  500) Compare =  500;
	TIM_SetCompare1(TIM4, Compare);
}

/**
  * 函    数：设置 TIM4_CH2 占空比（ESC 脉宽）
  * 参    数：Compare：500~1000（1000~2000μs）
  */
void PWM4_SetCompare2(uint16_t Compare)
{
	if(Compare > 1000) Compare = 1000;
	if(Compare <  500) Compare =  500;
	TIM_SetCompare2(TIM4, Compare);
}

/**
  * 函    数：设置 TIM4_CH3 占空比（ESC 脉宽）
  * 参    数：Compare：500~1000（1000~2000μs）
  */
void PWM4_SetCompare3(uint16_t Compare)
{
	if(Compare > 1000) Compare = 1000;
	if(Compare <  500) Compare =  500;
	TIM_SetCompare3(TIM4, Compare);
}

/**
  * 函    数：设置 TIM4_CH4 占空比（ESC 脉宽）
  * 参    数：Compare：500~1000（1000~2000μs）
  */
void PWM4_SetCompare4(uint16_t Compare)
{
	if(Compare > 1000) Compare = 1000;
	if(Compare <  500) Compare =  500;
	TIM_SetCompare4(TIM4, Compare);
}
