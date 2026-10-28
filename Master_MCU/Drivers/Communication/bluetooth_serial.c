#include "event_notify.h"
/**
  ******************************************************************************
  * @file    bluetooth_serial.c
  * @brief   蓝牙调参串口驱动 (USART1 DMA 模式)
  *
  *          TX: DMA1_Channel4  Normal  模式 — 批量发送不阻塞
  *          RX: DMA1_Channel5  Circular 模式 + USART1 IDLE 中断
  *          协议: [tag,param,val] 帧格式, 38400-8N1
  *
  *          PA9  = USART1_TX (AF_PP)
  *          PA10 = USART1_RX (IN_FLOATING)
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "bluetooth_serial.h"
#include "dma_rx.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ============================================
 * 全局变量
 * ============================================ */
char    bluetooth_rx_packet[100];
volatile uint8_t bluetooth_rx_ready;

/* ============================================
 * DMA 缓冲区
 * ============================================ */
#define BS_TX_BUF_SIZE      128u
#define BS_RX_DMA_BUF_SIZE  256u
#define BS_RX_SERVICE_BYTES 64u

static uint8_t bs_tx_buf[BS_TX_BUF_SIZE];
static uint8_t bs_rx_dma_buf[BS_RX_DMA_BUF_SIZE] __attribute__((aligned(4)));

static uint32_t tx_started_ms;
static uint8_t tx_timed;
static volatile uint32_t tx_errors;
static volatile uint8_t bs_tx_busy = 0;       /* DMA TX 忙标志（ISR 清零） */

/* RX 解析状态机 */
static uint8_t  bs_rx_state = 0;
static uint8_t  bs_rx_idx   = 0;
static dma_rx_t bs_rx;
static uint32_t bs_rx_frame_errors;

/* ============================================
 * 静态函数声明
 * ============================================ */
static void bluetooth_serial_dma_tx_config(void);
static void bluetooth_serial_dma_rx_config(void);
static void bluetooth_serial_feed_byte(uint8_t byte);


/* ============================================
 * bluetooth_serial_init
 * ============================================ */
void bluetooth_serial_init(void)
{
    GPIO_InitTypeDef gpio_config;
    USART_InitTypeDef usart_config;
    NVIC_InitTypeDef nvic_config;

    /* ── 时钟 ── */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    /* ── PA9: USART1_TX (复用推挽) ── */
    gpio_config.GPIO_Pin   = GPIO_Pin_9;
    gpio_config.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio_config.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio_config);

    /* ── PA10: USART1_RX (浮空输入 — DMA 接收推荐) ── */
    gpio_config.GPIO_Pin  = GPIO_Pin_10;
    gpio_config.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio_config);

    /* ── USART1: 38400-8N1 ── */
    usart_config.USART_BaudRate            = 38400;
    usart_config.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_config.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    usart_config.USART_Parity              = USART_Parity_No;
    usart_config.USART_StopBits            = USART_StopBits_1;
    usart_config.USART_WordLength          = USART_WordLength_8b;
    USART_Init(USART1, &usart_config);

    /* ── DMA 配置 ── */
    bluetooth_serial_dma_tx_config();
    bluetooth_serial_dma_rx_config();

    /* ── 使能 USART DMA 请求 ── */
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);

    /* ── 使能 IDLE 中断 (替代 RXNE) ── */
    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);

    /* ── NVIC: USART1 (IDLE 中断) ── */
    nvic_config.NVIC_IRQChannel                   = USART1_IRQn;
    nvic_config.NVIC_IRQChannelCmd                = ENABLE;
    nvic_config.NVIC_IRQChannelPreemptionPriority = 8u;
    nvic_config.NVIC_IRQChannelSubPriority        = 0u;
    NVIC_Init(&nvic_config);

    /* ── NVIC: DMA1_Channel4 (TX 完成中断) ── */
    nvic_config.NVIC_IRQChannel                   = DMA1_Channel4_IRQn;
    nvic_config.NVIC_IRQChannelCmd                = ENABLE;
    nvic_config.NVIC_IRQChannelPreemptionPriority = 8u;
    nvic_config.NVIC_IRQChannelSubPriority        = 0u;
    NVIC_Init(&nvic_config);

    /* DMA RX events also handle streams without IDLE gaps. */
    nvic_config.NVIC_IRQChannel = DMA1_Channel5_IRQn;
    nvic_config.NVIC_IRQChannelSubPriority = 0u;
    NVIC_Init(&nvic_config);

    /* ── 启动 USART1 ── */
    USART_Cmd(USART1, ENABLE);
}

/* ============================================
 * DMA1_Channel4 — USART1_TX
 * Normal 模式，每次发送时配置长度
 * ============================================ */
static void bluetooth_serial_dma_tx_config(void)
{
    DMA_InitTypeDef dma_config;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA1_Channel4);

    dma_config.DMA_PeripheralBaseAddr = (uint32_t)&(USART1->DR);
    dma_config.DMA_MemoryBaseAddr     = (uint32_t)bs_tx_buf;
    dma_config.DMA_DIR                = DMA_DIR_PeripheralDST;  /* 内存→外设 */
    dma_config.DMA_BufferSize         = 0;                      /* 每次发送时设定 */
    dma_config.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    dma_config.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    dma_config.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma_config.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    dma_config.DMA_Mode               = DMA_Mode_Normal;
    dma_config.DMA_Priority           = DMA_Priority_Medium;
    dma_config.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel4, &dma_config);

    /* 使能传输完成中断 */
    DMA_ITConfig(DMA1_Channel4, DMA_IT_TC | DMA_IT_TE, ENABLE);
}

/* ============================================
 * DMA1_Channel5 — USART1_RX
 * Circular 模式，持续接收
 * ============================================ */
static void bluetooth_serial_dma_rx_config(void)
{
    DMA_InitTypeDef dma_config;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA1_Channel5);

    dma_config.DMA_PeripheralBaseAddr = (uint32_t)&(USART1->DR);
    dma_config.DMA_MemoryBaseAddr     = (uint32_t)bs_rx_dma_buf;
    dma_config.DMA_DIR                = DMA_DIR_PeripheralSRC;  /* 外设→内存 */
    dma_config.DMA_BufferSize         = BS_RX_DMA_BUF_SIZE;
    dma_config.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    dma_config.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    dma_config.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma_config.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    dma_config.DMA_Mode               = DMA_Mode_Circular;
    dma_config.DMA_Priority           = DMA_Priority_High;
    dma_config.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel5, &dma_config);

    dma_rx_init(&bs_rx, DMA1_Channel5, bs_rx_dma_buf,
               BS_RX_DMA_BUF_SIZE, DMA1_FLAG_HT5, DMA1_FLAG_TC5);
    DMA_ITConfig(DMA1_Channel5, DMA_IT_HT | DMA_IT_TC, ENABLE);
    bs_rx_state = 0u;
    bs_rx_idx = 0u;
    bs_rx_frame_errors = 0u;
    bluetooth_rx_ready = 0u;

    /* 立即启动 DMA 接收 */
    DMA_Cmd(DMA1_Channel5, ENABLE);
}

/* ============================================
 * DMA1_Channel4 中断 — TX 完成
 * ============================================ */
void DMA1_Channel4_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TE4) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_TE4);DMA_Cmd(DMA1_Channel4,DISABLE);bs_tx_busy=0u;tx_errors++;
    }
    if (DMA_GetITStatus(DMA1_IT_TC4) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TC4);
        bs_tx_busy = 0;
    }
}

/* ============================================
 * USART1 中断 — IDLE 只发布事件，不作为协议帧边界
 * ============================================ */
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_IDLE) != RESET)
    {
        /* 清除 IDLE 标志：先读 SR，再读 DR */
        volatile uint32_t tmp = USART1->SR;
        tmp = USART1->DR;
        (void)tmp;

        dma_rx_notify_from_isr(&bs_rx);
    }
}

void DMA1_Channel5_IRQHandler(void)
{
    dma_rx_on_dma_interrupt(&bs_rx);
}

/* Main loop only. Stop after one complete command so the caller applies
 * it before continuing; subsequent bytes stay in the DMA ring.
 */
uint8_t bluetooth_serial_process(void)
{
    uint8_t byte;
    uint8_t dropped;
    uint16_t budget;

    if (bluetooth_rx_ready != 0u) return 1u;
    for (budget = 0u; budget < BS_RX_SERVICE_BYTES; ++budget) {
        uint16_t count = dma_rx_read(&bs_rx, &byte, 1u, &dropped);
        if (dropped != 0u) {
            bs_rx_state = 0u;
            bs_rx_idx = 0u;
        }
        if (count == 0u) break;
        bluetooth_serial_feed_byte(byte);
        if (bluetooth_rx_ready != 0u) return 1u;
    }
    return 0u;
}

uint32_t bluetooth_serial_get_rx_overruns(void)
{
    return bs_rx.overruns;
}

uint32_t bluetooth_serial_get_rx_frame_errors(void)
{
    return bs_rx_frame_errors;
}

/* Frame assembly runs only in the main loop. A new '[' resynchronizes
 * a partial frame. An overlength command is discarded, never truncated.
 */
static void bluetooth_serial_feed_byte(uint8_t byte)
{
    if (byte == '[') {
        bs_rx_state = 1u;
        bs_rx_idx = 0u;
        return;
    }
    if (bs_rx_state == 0u) return;
    if (byte == ']') {
        bs_rx_state = 0u;
        bluetooth_rx_packet[bs_rx_idx] = '\0';
        bluetooth_rx_ready = 1u;
    } else if (bs_rx_idx < sizeof(bluetooth_rx_packet) - 1u) {
        bluetooth_rx_packet[bs_rx_idx++] = (char)byte;
    } else {
        bs_rx_frame_errors++;
        bs_rx_state = 0u;
        bs_rx_idx = 0u;
    }
}

/* ============================================
 * 等待 DMA TX 空闲
 * ============================================ */
static void bluetooth_serial_wait_tx_idle(void)
{
    while (bs_tx_busy)
    {
        /* 等待 DMA TC 中断清零 bs_tx_busy */
    }
}

/* ============================================
 * bluetooth_serial_send_dma — DMA 批量发送（主循环使用）
 *
 * 非阻塞：启动 DMA 后立即返回
 * 若上次发送未完成则跳过本次遥测，避免阻塞控制循环
 * ============================================ */
void bluetooth_serial_send_dma(uint8_t *buffer, uint16_t length)
{
    if (length == 0 || length > BS_TX_BUF_SIZE)
        return;

    if (bs_tx_busy != 0u) return;

    /* 拷贝数据到 DMA 缓冲区（DMA 需要数据在传输期间保持有效） */
    memcpy(bs_tx_buf, buffer, length);

    /* 重配 DMA 并启动 */
    DMA_Cmd(DMA1_Channel4, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_GL4);
    DMA_SetCurrDataCounter(DMA1_Channel4, length);
    bs_tx_busy = 1;
    DMA_Cmd(DMA1_Channel4, ENABLE);
}

/* ============================================
 * bluetooth_serial_send_byte — 单字节阻塞发送（调试用）
 *
 * 等待 DMA TX 空闲 + 移位寄存器空，然后轮询发送
 * ============================================ */
void bluetooth_serial_send_byte(uint8_t byte)
{
    bluetooth_serial_wait_tx_idle();

    /* 等待移位寄存器清空（上一帧最后一字节可能还在移位） */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);

    USART_SendData(USART1, byte);

    /* 等待数据送入移位寄存器 */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
}

/* ============================================
 * 基础发送接口
 * ============================================ */

void bluetooth_serial_send_array(uint8_t *array, uint16_t length)
{
    uint16_t i;
    for (i = 0; i < length; i++)
    {
        bluetooth_serial_send_byte(array[i]);
    }
}

void bluetooth_serial_send_string(char *string)
{
    uint8_t i;
    for (i = 0; string[i] != '\0'; i++)
    {
        bluetooth_serial_send_byte(string[i]);
    }
}

uint32_t bluetooth_serial_pow(uint32_t base, uint32_t exponent)
{
    uint32_t result = 1;
    while (exponent--)
    {
        result *= base;
    }
    return result;
}

void bluetooth_serial_send_number(uint32_t number, uint8_t length)
{
    uint8_t i;
    for (i = 0; i < length; i++)
    {
        bluetooth_serial_send_byte(number / bluetooth_serial_pow(10, length - i - 1) % 10 + '0');
    }
}

void bluetooth_serial_printf(char *format, ...)
{
    char string[100];
    va_list arg;
    va_start(arg, format);
    vsnprintf(string, sizeof(string), format, arg);
    va_end(arg);
    bluetooth_serial_send_string(string);
}

int bluetooth_serial_take_command(flight_command_t *command)
{
    int ok;
    if(!bluetooth_serial_process())return 0;
    ok=flight_command_parse(bluetooth_rx_packet,command);
    bluetooth_rx_ready=0u;bluetooth_rx_packet[0]='\0';
    return ok?1:-1;
}
uint8_t bluetooth_serial_try_send(uint8_t *data,uint16_t length)
{
    if(!length || length>BS_TX_BUF_SIZE || bs_tx_busy)return 0u;
    bluetooth_serial_send_dma(data,length);tx_timed=0u;return 1u;
}
void bluetooth_serial_service_tx(uint32_t now_ms)
{
    if(!bs_tx_busy){tx_timed=0u;return;}
    if(!tx_timed){tx_started_ms=now_ms;tx_timed=1u;}
    if((uint32_t)(now_ms-tx_started_ms)>=20u) {
        DMA_Cmd(DMA1_Channel4,DISABLE);DMA_ClearFlag(DMA1_FLAG_GL4);
        bs_tx_busy=0u;tx_timed=0u;tx_errors++;
    }
}
