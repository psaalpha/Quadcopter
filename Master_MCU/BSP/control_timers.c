#include "control_timers.h"

#include "board_config.h"
#include "stm32f10x.h"

#define CONTROL_TIMER_INPUT_HZ       72000000u
#define CONTROL_TIMER_COUNTER_HZ     10000u
#define CONTROL_TIMER_PRESCALER      \
    (CONTROL_TIMER_INPUT_HZ / CONTROL_TIMER_COUNTER_HZ)
#define CONTROL_TIMER_TICKS_PER_MS   \
    (CONTROL_TIMER_COUNTER_HZ / 1000u)

static void ConfigureTimeBase(TIM_TypeDef *timer, uint16_t period_ms)
{
    TIM_TimeBaseInitTypeDef time_base;

    TIM_TimeBaseStructInit(&time_base);
    time_base.TIM_ClockDivision = TIM_CKD_DIV1;
    time_base.TIM_CounterMode = TIM_CounterMode_Up;
    time_base.TIM_Period =
        (uint16_t)(period_ms * CONTROL_TIMER_TICKS_PER_MS - 1u);
    time_base.TIM_Prescaler = (uint16_t)(CONTROL_TIMER_PRESCALER - 1u);
    time_base.TIM_RepetitionCounter = 0u;
    TIM_TimeBaseInit(timer, &time_base);
    TIM_ClearITPendingBit(timer, TIM_IT_Update);
    TIM_ITConfig(timer, TIM_IT_Update, ENABLE);
}

static void ConfigureInterrupt(IRQn_Type interrupt,
                               uint8_t preemption_priority,
                               uint8_t sub_priority)
{
    NVIC_InitTypeDef interrupt_config;

    interrupt_config.NVIC_IRQChannel = interrupt;
    interrupt_config.NVIC_IRQChannelPreemptionPriority =
        preemption_priority;
    interrupt_config.NVIC_IRQChannelSubPriority = sub_priority;
    interrupt_config.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&interrupt_config);
}

void BoardControlTimers_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_TIM2 | RCC_APB1Periph_TIM3, ENABLE);

    TIM_InternalClockConfig(TIM1);
    TIM_InternalClockConfig(TIM2);
    TIM_InternalClockConfig(TIM3);

    ConfigureTimeBase(TIM1, BOARD_RC_TASK_PERIOD_MS);
    ConfigureTimeBase(TIM2, BOARD_IMU_TASK_PERIOD_MS);
    ConfigureTimeBase(TIM3, BOARD_ANGLE_TASK_PERIOD_MS);

    ConfigureInterrupt(TIM2_IRQn, 1u, 1u);
    ConfigureInterrupt(TIM3_IRQn, 1u, 3u);
    ConfigureInterrupt(TIM1_UP_IRQn, 2u, 2u);

    TIM_Cmd(TIM1, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}
