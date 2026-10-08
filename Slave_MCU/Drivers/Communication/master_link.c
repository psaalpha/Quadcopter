#include "stm32f10x.h"
#include "master_link.h"
#include <string.h>
#define LINK_DMA_DONE 1u
#define LINK_DMA_ERROR 2u
#define LINK_DMA_TIMEOUT_MS 20u
static uint8_t tx_frame[INTER_MCU_FRAME_SIZE];
static volatile uint8_t tx_events;
static uint8_t tx_busy;
static uint32_t tx_started_ms;
static master_link_stats_t statistics;
void master_link_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef serial;
    DMA_InitTypeDef dma;
    NVIC_InitTypeDef interrupt;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    gpio.GPIO_Pin = GPIO_Pin_2; /* PA3 belongs to the buzzer. */
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    USART_StructInit(&serial);
    serial.USART_BaudRate = 115200u;
    serial.USART_Mode = USART_Mode_Tx;
    USART_Init(USART2, &serial);
    DMA_DeInit(DMA1_Channel7);
    DMA_StructInit(&dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&USART2->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)tx_frame;
    dma.DMA_DIR = DMA_DIR_PeripheralDST;
    dma.DMA_BufferSize = INTER_MCU_FRAME_SIZE;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_Mode = DMA_Mode_Normal;
    dma.DMA_Priority = DMA_Priority_Medium;
    DMA_Init(DMA1_Channel7, &dma);
    DMA_ClearFlag(DMA1_FLAG_GL7);
    DMA_ITConfig(DMA1_Channel7, DMA_IT_TC | DMA_IT_TE, ENABLE);
    interrupt.NVIC_IRQChannel = DMA1_Channel7_IRQn;
    interrupt.NVIC_IRQChannelPreemptionPriority = 1u;
    interrupt.NVIC_IRQChannelSubPriority = 2u;
    interrupt.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&interrupt);
    tx_events = 0u;
    tx_busy = 0u;
    memset(&statistics, 0, sizeof(statistics));
    USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE);
    USART_Cmd(USART2, ENABLE);
}
void DMA1_Channel7_IRQHandler(void)
{
    if (DMA_GetFlagStatus(DMA1_FLAG_TE7) != RESET) {
        DMA_ClearFlag(DMA1_FLAG_TE7);
        tx_events |= LINK_DMA_ERROR;
    }
    if (DMA_GetFlagStatus(DMA1_FLAG_TC7) != RESET) {
        DMA_ClearFlag(DMA1_FLAG_TC7);
        tx_events |= LINK_DMA_DONE;
    }
}
void master_link_process(uint32_t now_ms)
{
    uint8_t events;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    events = tx_events;
    tx_events = 0u;
    __set_PRIMASK(mask);
    if (tx_busy == 0u) return;
    if ((events != 0u) || ((uint32_t)(now_ms - tx_started_ms) >= LINK_DMA_TIMEOUT_MS)) {
        DMA_Cmd(DMA1_Channel7, DISABLE);
        DMA_ClearFlag(DMA1_FLAG_GL7);
        tx_busy = 0u;
        if (((events & LINK_DMA_ERROR) != 0u) || (events == 0u)) statistics.dma_errors++;
        else statistics.sent++;
        /* DMA TC frees memory. USART TC (last stop bit) may follow later;
         * a subsequent DMA still waits for TXE and cannot overwrite DR.
         */
    }
}
uint8_t master_link_send(const inter_mcu_sensor_data_t *packet, uint32_t now_ms)
{
    master_link_process(now_ms);
    if (tx_busy != 0u) {
        statistics.skipped_busy++;
        return 0u;
    }
    if (inter_mcu_encode_sensor_frame(packet, tx_frame, sizeof(tx_frame)) == 0u) {
        statistics.encode_errors++;
        return 0u;
    }
    DMA_Cmd(DMA1_Channel7, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_GL7);
    DMA_SetCurrDataCounter(DMA1_Channel7, sizeof(tx_frame));
    tx_started_ms = now_ms;
    tx_busy = 1u;
    DMA_Cmd(DMA1_Channel7, ENABLE);
    return 1u;
}
uint8_t master_link_is_busy(void) { return tx_busy; }
void master_link_get_stats(master_link_stats_t *stats) { if (stats != 0) *stats = statistics; }
