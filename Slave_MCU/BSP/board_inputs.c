#include "board_inputs.h"
static volatile uint8_t key_pending;
static volatile uint8_t servo_pending;
static volatile uint8_t calib_pulse_pending;
static uint8_t calib_saw_low;
static void board_inputs_configure_line(uint32_t line, uint8_t pin_source,
                          EXTITrigger_TypeDef trigger, IRQn_Type irq)
{
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef interrupt;
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, pin_source);
    EXTI_ClearITPendingBit(line);
    exti.EXTI_Line = line;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = trigger;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);
    interrupt.NVIC_IRQChannel = irq;
    interrupt.NVIC_IRQChannelPreemptionPriority = 1u;
    interrupt.NVIC_IRQChannelSubPriority = 3u;
    interrupt.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&interrupt);
}
void board_inputs_init(void)
{
    GPIO_InitTypeDef gpio;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    key_pending = 0u;
    servo_pending = 1u; /* Publish the initial level too. */
    calib_pulse_pending = 0u;
    calib_saw_low = 0u; /* Startup-low alone is not a 1->0->1 pulse. */
    board_inputs_configure_line(EXTI_Line0, GPIO_PinSource0, EXTI_Trigger_Falling, EXTI0_IRQn);
    board_inputs_configure_line(EXTI_Line1, GPIO_PinSource1, EXTI_Trigger_Rising_Falling, EXTI1_IRQn);
    board_inputs_configure_line(EXTI_Line8, GPIO_PinSource8, EXTI_Trigger_Rising_Falling, EXTI9_5_IRQn);
}
void EXTI0_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line0);
        key_pending = 1u;
    }
}
void EXTI1_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line1) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line1);
        servo_pending = 1u;
    }
}
void EXTI9_5_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line8) != RESET) {
        EXTI_ClearITPendingBit(EXTI_Line8);
        /* Capture only edge history: a whole pulse between two main-loop
         * turns must survive. Calibration itself stays in the main loop.
         */
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_8) == 0u) calib_saw_low = 1u;
        else if (calib_saw_low != 0u) {
            calib_saw_low = 0u;
            calib_pulse_pending = 1u;
        }
    }
}
void board_inputs_take(board_input_events_t *events)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    events->key = key_pending;
    events->servo_changed = servo_pending;
    events->calibration_pulse = calib_pulse_pending;
    key_pending = 0u;
    servo_pending = 0u;
    calib_pulse_pending = 0u;
    __set_PRIMASK(mask);
}
uint8_t board_inputs_servo_level(void) { return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1); }

uint8_t board_inputs_key_level(void) { return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0); }
