#include "control_timers.h"
#include "stm32f10x.h"
#include "millisecond_clock.h"
void board_control_timers_init(void)
{
    TIM_TimeBaseInitTypeDef time_base;
    NVIC_InitTypeDef interrupt_config;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_InternalClockConfig(TIM2);
    TIM_TimeBaseStructInit(&time_base);
    time_base.TIM_Prescaler = 72u - 1u;
    time_base.TIM_Period = 1000u - 1u;
    TIM_TimeBaseInit(TIM2, &time_base);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    interrupt_config.NVIC_IRQChannel = TIM2_IRQn;
    interrupt_config.NVIC_IRQChannelPreemptionPriority = 1u;
    interrupt_config.NVIC_IRQChannelSubPriority = 1u;
    interrupt_config.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&interrupt_config);
    millisecond_clock_init();
    TIM_Cmd(TIM2, ENABLE);
}
