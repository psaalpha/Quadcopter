#include "event_notify.h"
#include "optical_flow.h"
#include "dma_rx.h"
#include "cv0850.h"
#include <string.h>
#define RX_DMA_SIZE 256u
#define RX_BATCH_SIZE 64u
#define STALE_MS 200u
#define DATA_VALID_FLAG 0xF5u
optical_flow_data_t optical_flow_data;
volatile uint8_t optical_flow_rx_ready;
static volatile uint8_t dma_buffer[RX_DMA_SIZE];
static dma_rx_t receiver;
static cv0850_t parser;
static uint32_t last_byte_ms;
static uint32_t last_flow_ms;
static uint32_t last_distance_ms;
static uint8_t have_flow;
static uint8_t have_distance;
static uint8_t paused;
static volatile uint8_t hardware_error;
static optical_flow_stats_t statistics;
static void optical_flow_reset_parser(void)
{
    parser.length = 0u;
    parser.needed = 0u;
}
static void optical_flow_invalidate(void)
{
    if (have_flow != 0u) optical_flow_data.epoch++;
    have_flow = 0u;
    have_distance = 0u;
    optical_flow_data.data_valid = 0u;
    optical_flow_data.distance = 0u;
    optical_flow_data.signal_strength = 0u;
    optical_flow_data.checksum_valid = 0u;
}
static void optical_flow_start_receiver(void)
{
    volatile uint32_t clear;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    USART_DMACmd(USART1, USART_DMAReq_Rx, DISABLE);
    DMA_Cmd(DMA1_Channel5, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_GL5);
    clear = USART1->SR;
    clear = USART1->DR;
    (void)clear;
    dma_rx_init(&receiver, DMA1_Channel5, dma_buffer, RX_DMA_SIZE,
               DMA1_FLAG_HT5, DMA1_FLAG_TC5);
    DMA_SetCurrDataCounter(DMA1_Channel5, RX_DMA_SIZE);
    hardware_error = 0u;
    optical_flow_reset_parser();
    DMA_Cmd(DMA1_Channel5, ENABLE);
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);
    __set_PRIMASK(mask);
}
void optical_flow_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef serial;
    DMA_InitTypeDef dma;
    NVIC_InitTypeDef interrupt;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    /* Keep existing PA9 mapping; the receive path uses only PA10. */
    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);
    USART_StructInit(&serial);
    serial.USART_BaudRate = 115200u;
    serial.USART_Mode = USART_Mode_Rx;
    USART_Init(USART1, &serial);
    DMA_DeInit(DMA1_Channel5);
    DMA_StructInit(&dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)dma_buffer;
    dma.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma.DMA_BufferSize = RX_DMA_SIZE;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_Mode = DMA_Mode_Circular;
    dma.DMA_Priority = DMA_Priority_High;
    DMA_Init(DMA1_Channel5, &dma);
    DMA_ITConfig(DMA1_Channel5, DMA_IT_HT | DMA_IT_TC | DMA_IT_TE, ENABLE);
    interrupt.NVIC_IRQChannel = USART1_IRQn;
    interrupt.NVIC_IRQChannelPreemptionPriority = 7u;
    interrupt.NVIC_IRQChannelSubPriority = 0u;
    interrupt.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&interrupt);
    interrupt.NVIC_IRQChannel = DMA1_Channel5_IRQn;
    interrupt.NVIC_IRQChannelSubPriority = 0u;
    NVIC_Init(&interrupt);
    memset(&optical_flow_data, 0, sizeof(optical_flow_data));
    memset(&statistics, 0, sizeof(statistics));
    cv0850_init(&parser);
    optical_flow_rx_ready = 0u;
    paused = 0u;
    have_flow = 0u;
    have_distance = 0u;
    USART_Cmd(USART1, ENABLE);
    optical_flow_start_receiver();
    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);
    USART_ITConfig(USART1, USART_IT_ERR, ENABLE);
}
void USART1_IRQHandler(void)
{
    uint32_t status = USART1->SR;
    if ((status & (USART_FLAG_IDLE | USART_FLAG_ORE | USART_FLAG_NE |
                   USART_FLAG_FE | USART_FLAG_PE)) != 0u) {
        volatile uint32_t clear = USART1->DR; /* SR then DR clears these flags. */
        (void)clear;
        if ((status & (USART_FLAG_ORE | USART_FLAG_NE | USART_FLAG_FE |
                       USART_FLAG_PE)) != 0u) hardware_error = 1u;
        dma_rx_notify_from_isr(&receiver);
    }
    rtos_notify_service_from_isr(RTOS_EVENT_IO);
}
void DMA1_Channel5_IRQHandler(void)
{
    if (DMA_GetFlagStatus(DMA1_FLAG_TE5) != RESET) {
        DMA_ClearFlag(DMA1_FLAG_TE5);
        hardware_error = 1u;
    }
    dma_rx_on_dma_interrupt(&receiver);
    rtos_notify_service_from_isr(RTOS_EVENT_IO);
}
/* CV0850 native output and MSPv2 are validated before publication.
 * Accumulate processed angular increments; every sensor frame contributes,
 * including frames between the slower 20Hz inter-MCU publications.
 */
static int32_t add_delta(int32_t previous, int32_t delta)
{
    uint32_t bits = (uint32_t)previous + (uint32_t)delta;
    int32_t result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}
static void optical_flow_feed_byte(uint8_t byte, uint32_t now_ms)
{
    uint32_t errors = parser.checksum_errors;
    uint8_t events = cv0850_feed(&parser, byte);
    if (errors != parser.checksum_errors) optical_flow_invalidate();
    if (events & CV0850_RANGE) {
        optical_flow_data.distance = parser.distance_mm;
        optical_flow_data.signal_strength = parser.quality;
        optical_flow_data.firmware_version = parser.version;
        have_distance = (uint8_t)(parser.distance_mm != 0u);
        last_distance_ms = now_ms;
        statistics.distance_frames++;
    }
    if (events & CV0850_FLOW) {
        if (parser.valid != 0u) {
            optical_flow_data.flow_x = add_delta(optical_flow_data.flow_x, parser.delta_x);
            optical_flow_data.flow_y = add_delta(optical_flow_data.flow_y, parser.delta_y);
            optical_flow_data.integration_us += parser.integration_us;
            optical_flow_data.data_valid = 1u;
            optical_flow_data.checksum_valid = 1u;
            have_flow = 1u;
            last_flow_ms = now_ms;
        } else {
            if (have_flow != 0u) optical_flow_data.epoch++;
            have_flow = 0u;
            optical_flow_data.data_valid = 0u;
        }
        statistics.flow_frames++;
    }
    if (events != 0u) optical_flow_rx_ready = 1u;
}
void optical_flow_process(uint32_t now_ms)
{
    uint8_t bytes[RX_BATCH_SIZE];
    uint8_t dropped;
    uint8_t error;
    uint16_t count;
    uint16_t i;
    uint32_t mask;
    if (paused != 0u) return;
    mask = __get_PRIMASK();
    __disable_irq();
    error = hardware_error;
    hardware_error = 0u;
    __set_PRIMASK(mask);
    if (error != 0u) {
        statistics.hardware_errors++;
        statistics.buffer_overruns += receiver.overruns;
        optical_flow_invalidate();
        optical_flow_start_receiver();
        return;
    }
    count = dma_rx_read(&receiver, bytes, sizeof(bytes), &dropped);
    if (dropped != 0u) {
        optical_flow_reset_parser();
        optical_flow_invalidate();
    }
    if (parser.length != 0u && (uint32_t)(now_ms - last_byte_ms) >= STALE_MS) optical_flow_reset_parser();
    for (i = 0u; i < count; ++i) optical_flow_feed_byte(bytes[i], now_ms);
    if (count != 0u) last_byte_ms = now_ms;
    optical_flow_timeout_check(now_ms);
}
void optical_flow_timeout_check(uint32_t now_ms)
{
    if (have_flow == 0u || (uint32_t)(now_ms - last_flow_ms) >= STALE_MS) {
        optical_flow_data.data_valid = 0u;
        if (have_flow != 0u) optical_flow_data.epoch++;
        have_flow = 0u;
    }
    if (have_distance == 0u || (uint32_t)(now_ms - last_distance_ms) >= STALE_MS) {
        optical_flow_data.distance = 0u;
        optical_flow_data.signal_strength = 0u;
    }
}
void optical_flow_pause(void)
{
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    USART_ITConfig(USART1, USART_IT_IDLE, DISABLE);
    USART_ITConfig(USART1, USART_IT_ERR, DISABLE);
    USART_DMACmd(USART1, USART_DMAReq_Rx, DISABLE);
    DMA_Cmd(DMA1_Channel5, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_GL5);
    paused = 1u;
    statistics.buffer_overruns += receiver.overruns;
    statistics.flash_pauses++;
    __set_PRIMASK(mask);
    optical_flow_invalidate();
    optical_flow_reset_parser();
}
void optical_flow_resume(void)
{
    optical_flow_start_receiver();
    paused = 0u;
    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);
    USART_ITConfig(USART1, USART_IT_ERR, ENABLE);
}
void optical_flow_get_stats(optical_flow_stats_t *stats)
{
    if (stats != 0) {
        *stats = statistics;
        stats->frame_errors += parser.format_errors;
        stats->checksum_errors = parser.checksum_errors;
        if (paused == 0u) stats->buffer_overruns += receiver.overruns;
    }
}
void optical_flow_get_snapshot(optical_flow_data_t *snapshot)
{
    if (snapshot != 0) *snapshot = optical_flow_data; /* Main owns all parsed fields. */
}
uint8_t optical_flow_has_new_data(void)
{
    uint8_t pending = optical_flow_rx_ready;
    optical_flow_rx_ready = 0u;
    return pending;
}
uint8_t optical_flow_is_valid(void) { return optical_flow_data.data_valid; }
int32_t optical_flow_get_x(void) { return optical_flow_data.flow_x; }
int32_t optical_flow_get_y(void) { return optical_flow_data.flow_y; }
uint16_t optical_flow_get_distance(void) { return optical_flow_data.distance; }
uint8_t optical_flow_get_signal_strength(void) { return optical_flow_data.signal_strength; }
