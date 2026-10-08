#include "stm32f10x.h"
#include "board_config.h"

/* 初始化 TIM4 四路 PWM，用于无刷电调 ESC。
 * 通道：CH1-PB6、CH2-PB7、CH3-PB8、CH4-PB9。
 * 频率：50Hz，20ms 周期。
 * 比较值：500~1000，对应 1000~2000us。
 */
void motor_pwm_init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitTypeDef gpio_config;
	gpio_config.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio_config.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9;
	gpio_config.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio_config);

	TIM_InternalClockConfig(TIM4);

	TIM_TimeBaseInitTypeDef timer_config;
	timer_config.TIM_ClockDivision = TIM_CKD_DIV1;
	timer_config.TIM_CounterMode = TIM_CounterMode_Up;
	timer_config.TIM_Period = 10000 - 1;
	timer_config.TIM_Prescaler = 144 - 1;
	timer_config.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM4, &timer_config);

	/* TIM4 generates PWM only; TIM2 schedules telemetry. */
	TIM_ITConfig(TIM4, TIM_IT_Update, DISABLE);

	TIM_OCInitTypeDef output_compare_config;
	TIM_OCStructInit(&output_compare_config);
	output_compare_config.TIM_OCMode = TIM_OCMode_PWM1;
	output_compare_config.TIM_OCPolarity = TIM_OCPolarity_High;
	output_compare_config.TIM_OutputState = TIM_OutputState_Enable;
	output_compare_config.TIM_Pulse = 500;

	TIM_OC1Init(TIM4, &output_compare_config);
	TIM_OC2Init(TIM4, &output_compare_config);
	TIM_OC3Init(TIM4, &output_compare_config);
	TIM_OC4Init(TIM4, &output_compare_config);
	/* 500Hz software updates take effect at the next 50Hz PWM boundary. */
	TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
	TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Enable);
	TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Enable);
	TIM_OC4PreloadConfig(TIM4, TIM_OCPreload_Enable);

	TIM_Cmd(TIM4, ENABLE);
}

/* Preserve the immediate minimum-output path used by flight safety.
 * Do not generate an update event: repeated locked RC frames must not
 * restart the PWM period. This retains the existing minimum pulse policy.
 */
void motor_pwm_set_minimum_output(void)
{
	TIM_UpdateDisableConfig(TIM4, ENABLE);
	TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Disable);
	TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Disable);
	TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Disable);
	TIM_OC4PreloadConfig(TIM4, TIM_OCPreload_Disable);
	TIM_SetCompare1(TIM4, BOARD_MOTOR_PWM_MIN_COMPARE);
	TIM_SetCompare2(TIM4, BOARD_MOTOR_PWM_MIN_COMPARE);
	TIM_SetCompare3(TIM4, BOARD_MOTOR_PWM_MIN_COMPARE);
	TIM_SetCompare4(TIM4, BOARD_MOTOR_PWM_MIN_COMPARE);
	TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
	TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Enable);
	TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Enable);
	TIM_OC4PreloadConfig(TIM4, TIM_OCPreload_Enable);
	/* Also overwrite the pending preload values so old flight commands
	 * cannot be latched by a later update event. */
	TIM_SetCompare1(TIM4, BOARD_MOTOR_PWM_MIN_COMPARE);
	TIM_SetCompare2(TIM4, BOARD_MOTOR_PWM_MIN_COMPARE);
	TIM_SetCompare3(TIM4, BOARD_MOTOR_PWM_MIN_COMPARE);
	TIM_SetCompare4(TIM4, BOARD_MOTOR_PWM_MIN_COMPARE);
	TIM_UpdateDisableConfig(TIM4, DISABLE);
}

/* 设置 TIM4_CH1 的 ESC 脉宽，compare 范围限制为 500~1000。 */
void motor_pwm_set_compare1(uint16_t compare)
{
	if(compare > 1000) compare = 1000;
	if(compare <  500) compare =  500;
	TIM_SetCompare1(TIM4, compare);
}

/* 设置 TIM4_CH2 的 ESC 脉宽，compare 范围限制为 500~1000。 */
void motor_pwm_set_compare2(uint16_t compare)
{
	if(compare > 1000) compare = 1000;
	if(compare <  500) compare =  500;
	TIM_SetCompare2(TIM4, compare);
}

/* 设置 TIM4_CH3 的 ESC 脉宽，compare 范围限制为 500~1000。 */
void motor_pwm_set_compare3(uint16_t compare)
{
	if(compare > 1000) compare = 1000;
	if(compare <  500) compare =  500;
	TIM_SetCompare3(TIM4, compare);
}

/* 设置 TIM4_CH4 的 ESC 脉宽，compare 范围限制为 500~1000。 */
void motor_pwm_set_compare4(uint16_t compare)
{
	if(compare > 1000) compare = 1000;
	if(compare <  500) compare =  500;
	TIM_SetCompare4(TIM4, compare);
}
