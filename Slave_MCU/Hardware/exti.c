#include "stm32f10x.h"                  // Device header
#include "misc.h"

// ȫ�ֱ�־������������ʱ����Ҫ����ѭ��������ο����� volatile uint8_t key_pressed_flag = 0;
volatile uint8_t key_flag = 0;

void EXTI_Key_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
    EXTI_InitTypeDef EXTI_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;

    // 1. ʱ��ʹ��
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    // 2. GPIO ���ã�PA0 ��������
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 3. �� PA0 ӳ�䵽 EXTI Line0
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);

    // 4. EXTI ���ã��½��ش���
    EXTI_InitStruct.EXTI_Line = EXTI_Line0;
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;   // ����ʱ�½���
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStruct);

    // 5. NVIC ���ã����ȼ����ɸ�����Ҫ������
    NVIC_InitStruct.NVIC_IRQChannel = EXTI0_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
}
void EXTI0_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line0) != RESET)
    {
        // ���ñ�־������ѭ��ִ������������������ж��д��������߼���
        key_flag = 1;
        
        // ����жϱ�־
        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}


