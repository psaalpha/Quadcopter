#include "stm32f10x.h"
#include <string.h>
USART_TypeDef fake_usart1, fake_usart2, fake_usart3;
DMA_Channel_TypeDef fake_dma3, fake_dma4, fake_dma5, fake_dma7;
GPIO_TypeDef fake_gpioa, fake_gpiob;
TIM_TypeDef fake_tim4;
uint32_t fake_dma_flags, fake_interrupt_mask;
uint32_t __get_PRIMASK(void) {return fake_interrupt_mask;}
void __disable_irq(void) {fake_interrupt_mask=1u;}
void __set_PRIMASK(uint32_t mask) {fake_interrupt_mask=mask;}
FlagStatus DMA_GetFlagStatus(uint32_t flag) {return (fake_dma_flags & flag) ? SET : RESET;}
void DMA_ClearFlag(uint32_t flag) {fake_dma_flags &= ~flag;}
FlagStatus DMA_GetITStatus(uint32_t flag) {return DMA_GetFlagStatus(flag);}
void DMA_ClearITPendingBit(uint32_t flag) {DMA_ClearFlag(flag);}
uint16_t DMA_GetCurrDataCounter(DMA_Channel_TypeDef *channel) {return channel->remaining;}
void DMA_SetCurrDataCounter(DMA_Channel_TypeDef *channel, uint16_t count) {channel->remaining=count;}
void DMA_DeInit(DMA_Channel_TypeDef *channel) {memset(channel,0,sizeof(*channel));}
void DMA_Init(DMA_Channel_TypeDef *channel, DMA_InitTypeDef *config) {
 channel->size=(uint16_t)config->DMA_BufferSize;
 channel->remaining=channel->size;
 channel->memory=(uint8_t *)(uintptr_t)config->DMA_MemoryBaseAddr;
}
void DMA_Cmd(DMA_Channel_TypeDef *channel, FunctionalState state) {channel->enabled=(uint8_t)state;}
void DMA_ITConfig(DMA_Channel_TypeDef *channel, uint32_t bits, FunctionalState state) {(void)channel;(void)bits;(void)state;}
void RCC_APB2PeriphClockCmd(uint32_t bits, FunctionalState state) {(void)bits;(void)state;}
void RCC_APB1PeriphClockCmd(uint32_t bits, FunctionalState state) {(void)bits;(void)state;}
void RCC_AHBPeriphClockCmd(uint32_t bits, FunctionalState state) {(void)bits;(void)state;}
void GPIO_Init(GPIO_TypeDef *gpio, GPIO_InitTypeDef *config) {(void)gpio;(void)config;}
void NVIC_Init(NVIC_InitTypeDef *config) {(void)config;}
void USART_Init(USART_TypeDef *usart, USART_InitTypeDef *config) {(void)usart;(void)config;}
void USART_StructInit(USART_InitTypeDef *config) {memset(config,0,sizeof(*config));}
void USART_DMACmd(USART_TypeDef *usart, uint16_t bits, FunctionalState state) {(void)usart;(void)bits;(void)state;}
void USART_ITConfig(USART_TypeDef *usart, uint16_t bits, FunctionalState state) {(void)usart;(void)bits;(void)state;}
void USART_Cmd(USART_TypeDef *usart, FunctionalState state) {(void)usart;(void)state;}
FlagStatus USART_GetITStatus(USART_TypeDef *usart, uint16_t bits) {(void)bits;return usart->SR ? SET : RESET;}
FlagStatus USART_GetFlagStatus(USART_TypeDef *usart, uint16_t bits) {(void)usart;(void)bits;return SET;}
void USART_SendData(USART_TypeDef *usart, uint16_t byte) {usart->DR=byte;}
void TIM_InternalClockConfig(TIM_TypeDef *tim) {(void)tim;}
void TIM_TimeBaseInit(TIM_TypeDef *tim, TIM_TimeBaseInitTypeDef *config) {(void)tim;(void)config;}
void TIM_ITConfig(TIM_TypeDef *tim, uint16_t bits, FunctionalState state) {(void)tim;(void)bits;(void)state;}
void TIM_OCStructInit(TIM_OCInitTypeDef *config) {memset(config,0,sizeof(*config));}
void TIM_Cmd(TIM_TypeDef *tim, FunctionalState state) {(void)tim;(void)state;}
void TIM_UpdateDisableConfig(TIM_TypeDef *tim, FunctionalState state) {tim->update_disabled=(uint8_t)state;}
static void set_compare(TIM_TypeDef *tim, unsigned index, uint16_t value) {
 tim->pending[index]=value;
 if (!tim->preload[index]) tim->active[index]=value;
}
#define CHANNEL(N) \
 void TIM_OC##N##Init(TIM_TypeDef *tim, TIM_OCInitTypeDef *config) {set_compare(tim,N-1u,(uint16_t)config->TIM_Pulse);} \
 void TIM_OC##N##PreloadConfig(TIM_TypeDef *tim, uint16_t state) {tim->preload[N-1u]=(state==TIM_OCPreload_Enable);} \
 void TIM_SetCompare##N(TIM_TypeDef *tim, uint16_t value) {set_compare(tim,N-1u,value);}
CHANNEL(1)
CHANNEL(2)
CHANNEL(3)
CHANNEL(4)

uint32_t fake_exti_pending;
void DMA_StructInit(DMA_InitTypeDef *config) {memset(config,0,sizeof(*config));}
void GPIO_EXTILineConfig(uint8_t port, uint8_t pin) {(void)port;(void)pin;}
void EXTI_Init(EXTI_InitTypeDef *config) {(void)config;}
FlagStatus EXTI_GetITStatus(uint32_t line) {return (fake_exti_pending & line) != 0u ? SET : RESET;}
void EXTI_ClearITPendingBit(uint32_t line) {fake_exti_pending &= ~line;}
uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *gpio, uint16_t pin) {return (gpio->input & pin) != 0u;}
