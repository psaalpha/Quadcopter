#include "stm32f10x.h"                  // Device header
void PWM2_Init(void)
{
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);			// 修正注释：开启TIM1时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);			//开启GPIOA的时钟
	
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);							//PA8=TIM1_CH3，复用推挽输出	
	
	/*配置时钟源*/
	TIM_InternalClockConfig(TIM1);		//选择TIM1内部时钟
	
	/*时基单元初始化*/
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;				
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;     
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up; 
	TIM_TimeBaseInitStructure.TIM_Period = 100 - 1;                 
	TIM_TimeBaseInitStructure.TIM_Prescaler = 7200 - 1;               
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;            
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);             // 修正注释：配置TIM1的时基单元
	
//	/*输出比较初始化*/ 
//	TIM_OCInitTypeDef TIM_OCInitStructure;							
//	TIM_OCStructInit(&TIM_OCInitStructure);                        
//	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;               
//	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;       
//	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;   
//	TIM_OCInitStructure.TIM_Pulse = 0;							
//	TIM_OC1Init(TIM1, &TIM_OCInitStructure);                        //配置TIM1的输出比较通道3
//	
//	
//	/* 新增：开启CH3的输出比较预装载（关键！否则CCR修改不生效） */
//	TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);
	
	/* 新增：开启TIM1的ARR预装载（保证周期稳定） */
	TIM_ARRPreloadConfig(TIM1, ENABLE);
	
	/* 高级定时器必须：主输出使能（否则无PWM输出） */
	TIM_CtrlPWMOutputs(TIM1, ENABLE);
	
	/*TIM使能*/
	TIM_Cmd(TIM1, ENABLE);			// 修正注释：使能TIM1
	

}

//void PWM2_SetCompare3(uint16_t Compare)
//{
//	TIM_SetCompare1(TIM1, Compare);		//设置TIM1_CH3的CCR值
//}
