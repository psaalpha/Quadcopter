#ifndef SERIAL_TEST_STM32_H
#define SERIAL_TEST_STM32_H
/* Host-only register model. Does not model electrical timing or NVIC latency. */
#include <stdint.h>
#include <stddef.h>
typedef enum {RESET=0, SET=1} FlagStatus;
typedef enum {DISABLE=0, ENABLE=1} FunctionalState;
typedef struct {uint32_t SR, DR;} USART_TypeDef;
typedef struct {uint16_t remaining, size; uint8_t *memory; uint8_t enabled;} DMA_Channel_TypeDef;
typedef struct {uint32_t input;} GPIO_TypeDef;
typedef uint32_t IRQn_Type;
typedef uint32_t EXTITrigger_TypeDef;
typedef struct {uint32_t EXTI_Line, EXTI_Mode, EXTI_Trigger; FunctionalState EXTI_LineCmd;} EXTI_InitTypeDef;
typedef struct {uint16_t active[4], pending[4]; uint8_t preload[4], update_disabled;} TIM_TypeDef;
extern USART_TypeDef fake_usart1, fake_usart2, fake_usart3;
extern DMA_Channel_TypeDef fake_dma3, fake_dma4, fake_dma5, fake_dma7;
extern GPIO_TypeDef fake_gpioa, fake_gpiob;
extern TIM_TypeDef fake_tim4;
#define USART1 (&fake_usart1)
#define USART2 (&fake_usart2)
#define USART3 (&fake_usart3)
#define DMA1_Channel7 (&fake_dma7)
#define DMA1_Channel3 (&fake_dma3)
#define DMA1_Channel4 (&fake_dma4)
#define DMA1_Channel5 (&fake_dma5)
#define GPIOA (&fake_gpioa)
#define GPIOB (&fake_gpiob)
#define TIM4 (&fake_tim4)
typedef struct {uint32_t GPIO_Pin, GPIO_Mode, GPIO_Speed;} GPIO_InitTypeDef;
typedef struct {uint32_t USART_BaudRate, USART_HardwareFlowControl, USART_Mode, USART_Parity, USART_StopBits, USART_WordLength;} USART_InitTypeDef;
typedef struct {uint32_t NVIC_IRQChannel, NVIC_IRQChannelCmd, NVIC_IRQChannelPreemptionPriority, NVIC_IRQChannelSubPriority;} NVIC_InitTypeDef;
typedef struct {uint32_t DMA_PeripheralBaseAddr, DMA_MemoryBaseAddr, DMA_DIR, DMA_BufferSize, DMA_PeripheralInc, DMA_MemoryInc, DMA_PeripheralDataSize, DMA_MemoryDataSize, DMA_Mode, DMA_Priority, DMA_M2M;} DMA_InitTypeDef;
typedef struct {uint32_t TIM_ClockDivision, TIM_CounterMode, TIM_Period, TIM_Prescaler, TIM_RepetitionCounter;} TIM_TimeBaseInitTypeDef;
typedef struct {uint32_t TIM_OCMode, TIM_OCPolarity, TIM_OutputState, TIM_Pulse;} TIM_OCInitTypeDef;
#define DMA1_FLAG_HT3 (1u << 0)
#define DMA1_FLAG_TC3 (1u << 1)
#define DMA1_FLAG_HT5 (1u << 2)
#define DMA1_FLAG_TC5 (1u << 3)
#define DMA1_FLAG_GL4 (1u << 4)
#define DMA1_IT_TC4 DMA1_FLAG_GL4
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t mask);
FlagStatus DMA_GetFlagStatus(uint32_t flag);
void DMA_ClearFlag(uint32_t flag);
FlagStatus DMA_GetITStatus(uint32_t flag);
void DMA_ClearITPendingBit(uint32_t flag);
uint16_t DMA_GetCurrDataCounter(DMA_Channel_TypeDef *channel);
void DMA_SetCurrDataCounter(DMA_Channel_TypeDef *channel, uint16_t count);
void DMA_DeInit(DMA_Channel_TypeDef *channel);
void DMA_Init(DMA_Channel_TypeDef *channel, DMA_InitTypeDef *config);
void DMA_Cmd(DMA_Channel_TypeDef *channel, FunctionalState state);
void DMA_ITConfig(DMA_Channel_TypeDef *channel, uint32_t bits, FunctionalState state);
void RCC_APB2PeriphClockCmd(uint32_t bits, FunctionalState state);
void RCC_APB1PeriphClockCmd(uint32_t bits, FunctionalState state);
void RCC_AHBPeriphClockCmd(uint32_t bits, FunctionalState state);
void GPIO_Init(GPIO_TypeDef *gpio, GPIO_InitTypeDef *config);
void NVIC_Init(NVIC_InitTypeDef *config);
void USART_Init(USART_TypeDef *usart, USART_InitTypeDef *config);
void USART_StructInit(USART_InitTypeDef *config);
void USART_DMACmd(USART_TypeDef *usart, uint16_t bits, FunctionalState state);
void USART_ITConfig(USART_TypeDef *usart, uint16_t bits, FunctionalState state);
void USART_Cmd(USART_TypeDef *usart, FunctionalState state);
FlagStatus USART_GetITStatus(USART_TypeDef *usart, uint16_t bits);
FlagStatus USART_GetFlagStatus(USART_TypeDef *usart, uint16_t bits);
void USART_SendData(USART_TypeDef *usart, uint16_t byte);
void TIM_InternalClockConfig(TIM_TypeDef *tim);
void TIM_TimeBaseInit(TIM_TypeDef *tim, TIM_TimeBaseInitTypeDef *config);
void TIM_ITConfig(TIM_TypeDef *tim, uint16_t bits, FunctionalState state);
void TIM_OCStructInit(TIM_OCInitTypeDef *config);
void TIM_Cmd(TIM_TypeDef *tim, FunctionalState state);
void TIM_UpdateDisableConfig(TIM_TypeDef *tim, FunctionalState state);
#define DMA1_Channel7 (&fake_dma7)
#define DMA1_Channel3_IRQn 1u
#define DMA1_Channel4_IRQn 2u
#define DMA1_Channel5_IRQn 3u
#define DMA_DIR_PeripheralDST 4u
#define DMA_DIR_PeripheralSRC 5u
#define DMA_IT_HT 6u
#define DMA_IT_TC 7u
#define DMA_M2M_Disable 8u
#define DMA_MemoryDataSize_Byte 9u
#define DMA_MemoryInc_Enable 10u
#define DMA_Mode_Circular 11u
#define DMA_Mode_Normal 12u
#define DMA_PeripheralDataSize_Byte 13u
#define DMA_PeripheralInc_Disable 14u
#define DMA_Priority_High 15u
#define DMA_Priority_Medium 16u
#define GPIO_Mode_AF_PP 17u
#define GPIO_Mode_IN_FLOATING 18u
#define GPIO_Pin_10 (1u << 10)
#define GPIO_Pin_11 (1u << 11)
#define GPIO_Pin_6 (1u << 6)
#define GPIO_Pin_7 (1u << 7)
#define GPIO_Pin_8 (1u << 8)
#define GPIO_Pin_9 (1u << 9)
#define GPIO_Speed_50MHz 25u
#define RCC_AHBPeriph_DMA1 26u
#define RCC_APB1Periph_TIM4 27u
#define RCC_APB1Periph_USART3 28u
#define RCC_APB2Periph_GPIOA 29u
#define RCC_APB2Periph_GPIOB 30u
#define RCC_APB2Periph_USART1 31u
#define TIM4_IRQn 32u
#define TIM_CKD_DIV1 33u
#define TIM_CounterMode_Up 34u
#define TIM_IT_Update 35u
#define TIM_OCMode_PWM1 36u
#define TIM_OCPolarity_High 37u
#define TIM_OCPreload_Disable 38u
#define TIM_OCPreload_Enable 39u
#define TIM_OutputState_Enable 40u
#define USART1_IRQn 41u
#define USART2 (&fake_usart2)
#define USART3_IRQn 42u
#define USART_DMAReq_Rx 43u
#define USART_DMAReq_Tx 44u
#define USART_FLAG_TC 45u
#define USART_FLAG_TXE 46u
#define USART_HardwareFlowControl_None 47u
#define USART_IT_IDLE 48u
#define USART_Mode_Rx 49u
#define USART_Mode_Tx 50u
#define USART_Parity_No 51u
#define USART_StopBits_1 52u
#define USART_WordLength_8b 53u
void TIM_OC1Init(TIM_TypeDef *tim, TIM_OCInitTypeDef *config);
void TIM_OC1PreloadConfig(TIM_TypeDef *tim, uint16_t state);
void TIM_SetCompare1(TIM_TypeDef *tim, uint16_t value);
void TIM_OC2Init(TIM_TypeDef *tim, TIM_OCInitTypeDef *config);
void TIM_OC2PreloadConfig(TIM_TypeDef *tim, uint16_t state);
void TIM_SetCompare2(TIM_TypeDef *tim, uint16_t value);
void TIM_OC3Init(TIM_TypeDef *tim, TIM_OCInitTypeDef *config);
void TIM_OC3PreloadConfig(TIM_TypeDef *tim, uint16_t state);
void TIM_SetCompare3(TIM_TypeDef *tim, uint16_t value);
void TIM_OC4Init(TIM_TypeDef *tim, TIM_OCInitTypeDef *config);
void TIM_OC4PreloadConfig(TIM_TypeDef *tim, uint16_t state);
void TIM_SetCompare4(TIM_TypeDef *tim, uint16_t value);
#define GPIO_Pin_0 (1u << 0)
#define GPIO_Pin_1 (1u << 1)
#define GPIO_Pin_2 (1u << 2)
#define GPIO_Mode_IPU 60u
#define RCC_APB1Periph_USART2 61u
#define RCC_APB2Periph_AFIO 62u
#define DMA_IT_TE 64u
#define DMA1_Channel7_IRQn 65u
#define DMA1_FLAG_TE5 (1u << 5)
#define DMA1_FLAG_TC7 (1u << 6)
#define DMA1_FLAG_TE7 (1u << 7)
#define DMA1_FLAG_GL5 (DMA1_FLAG_HT5 | DMA1_FLAG_TC5 | DMA1_FLAG_TE5)
#define DMA1_FLAG_GL7 (DMA1_FLAG_TC7 | DMA1_FLAG_TE7)
#define USART_FLAG_IDLE (1u << 4)
#define USART_FLAG_ORE (1u << 3)
#define USART_FLAG_NE (1u << 2)
#define USART_FLAG_FE (1u << 1)
#define USART_FLAG_PE (1u << 0)
#define USART_IT_ERR 66u
#define EXTI_Line0 (1u << 0)
#define EXTI_Line1 (1u << 1)
#define EXTI_Line8 (1u << 8)
#define GPIO_PortSourceGPIOA 0u
#define GPIO_PinSource0 0u
#define GPIO_PinSource1 1u
#define GPIO_PinSource8 8u
#define EXTI0_IRQn 67u
#define EXTI1_IRQn 68u
#define EXTI9_5_IRQn 69u
#define EXTI_Trigger_Falling 0u
#define EXTI_Trigger_Rising_Falling 1u
#define EXTI_Mode_Interrupt 0u
void DMA_StructInit(DMA_InitTypeDef *config);
void GPIO_EXTILineConfig(uint8_t port, uint8_t pin);
void EXTI_Init(EXTI_InitTypeDef *config);
FlagStatus EXTI_GetITStatus(uint32_t line);
void EXTI_ClearITPendingBit(uint32_t line);
uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *gpio, uint16_t pin);
#endif
