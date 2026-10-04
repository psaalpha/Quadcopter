#include "slave_board.h"
#include "millisecond_clock.h"
#include "exti.h"
#define ADC_VREF 3.30f
#define ADC_VOLT_DIVIDER 4.0f
#define BUZZER_PORT GPIOA
#define BUZZER_PIN GPIO_Pin_3
#define IWDG_RELOAD_COUNT 625u
static uint8_t adc_ready;
static uint8_t adc_pending;
static uint8_t battery_valid;
static uint32_t adc_started_ms;
static uint32_t adc_errors;
static float battery_voltage;
static void IWDG_Init(void)
{
    /* 调试时冻结 IWDG，避免断点导致复位 */
    DBGMCU_Config(DBGMCU_IWDG_STOP, ENABLE);

    uint32_t timeout = 1000000u;
    /* 使能 LSI，有限等待就绪。 */
    RCC_LSICmd(ENABLE);
    while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET && timeout != 0u) timeout--;
    if (timeout == 0u) return;

    /* 解锁 IWDG 寄存器 */
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

    /* 配置：LSI/64 = 625Hz, 重装载 625 → 1 秒超时 */
    IWDG_SetPrescaler(IWDG_Prescaler_64);
    IWDG_SetReload(IWDG_RELOAD_COUNT);

    /* 喂狗并启动 */
    IWDG_ReloadCounter();
    IWDG_Enable();
}

/* 初始化 TIM3_CH3 舵机 PWM：PB0，50Hz，20ms 周期。 */
static void Servo_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* PB0 -> TIM3_CH3 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* TIM3: 72MHz / 72 = 1MHz，ARR=20000-1 -> 20ms。 */
    TIM_TimeBaseStructure.TIM_Prescaler         = 72 - 1;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period            = 20000 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 1500;     /* 默认中立位 1.5ms */
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC3Init(TIM3, &TIM_OCInitStructure);

    TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

void SlaveBoard_SetServo(uint16_t pulse_us)
{
    if (pulse_us < 500)  pulse_us = 500;
    if (pulse_us > 2500) pulse_us = 2500;
    TIM_SetCompare3(TIM3, pulse_us);
}

/* 初始化蜂鸣器：PA3，低电平驱动。 */
static void Buzzer_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = BUZZER_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BUZZER_PORT, &GPIO_InitStructure);
    GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
}

/* 初始化 ADC1_IN9：PB1 电池电压检测。 */
static void ADC_Battery_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;
    uint32_t timeout;

    RCC_ADCCLKConfig(RCC_PCLK2_Div6); /* 72MHz / 6 = 12MHz, within 14MHz limit. */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_ADC1, ENABLE);

    GPIO_StructInit(&GPIO_InitStructure);
    /* PB1 模拟输入。 */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* ADC1 单次转换，软件触发。 */
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_9, 1, ADC_SampleTime_55Cycles5);
    ADC_Cmd(ADC1, ENABLE);

    /* ADC 自校准。 */
    ADC_ResetCalibration(ADC1);
    timeout = 100000u;
    while (ADC_GetResetCalibrationStatus(ADC1) && timeout != 0u) timeout--;
    if (timeout == 0u) { adc_errors++; return; }
    ADC_StartCalibration(ADC1);
    timeout = 100000u;
    while (ADC_GetCalibrationStatus(ADC1) && timeout != 0u) timeout--;
    if (timeout == 0u) { adc_errors++; return; }
    adc_ready = 1u;
}

void SlaveBoard_StartTick(void)
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
    MillisecondClock_Init();
    TIM_Cmd(TIM2, ENABLE);
}

void SlaveBoard_InitInterrupts(void) { NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); }
void SlaveBoard_Init(void)
{
    Servo_Init();
    ADC_Battery_Init();
    Buzzer_Init();
    EXTI_Inputs_Init();
    IWDG_Init();
}
void SlaveBoard_FeedWatchdog(void) { IWDG_ReloadCounter(); }
void SlaveBoard_SetLowBattery(uint8_t low)
{
    if (low != 0u) GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
    else GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
}
void SlaveBoard_RequestBattery(uint32_t now_ms)
{
    if (adc_ready == 0u || adc_pending != 0u) return;
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    adc_started_ms = now_ms;
    adc_pending = 1u;
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}
void SlaveBoard_ProcessBattery(uint32_t now_ms)
{
    if (adc_pending == 0u) return;
    if (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) != RESET) {
        uint16_t value = ADC_GetConversionValue(ADC1);
        battery_voltage = (float)value / 4095.0f * ADC_VREF * ADC_VOLT_DIVIDER;
        battery_valid = 1u;
        adc_pending = 0u;
    } else if ((uint32_t)(now_ms - adc_started_ms) >= 5u) {
        adc_pending = 0u;
        battery_valid = 0u;
        adc_errors++;
        ADC_SoftwareStartConvCmd(ADC1, DISABLE);
    }
}
float SlaveBoard_BatteryVoltage(void) { return battery_voltage; }
uint8_t SlaveBoard_BatteryValid(void) { return battery_valid; }
uint32_t SlaveBoard_AdcErrors(void) { return adc_errors; }
