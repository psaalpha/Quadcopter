/**
  ******************************************************************************
  * @file    SlaveMCU.c
  * @brief   从控传感器 USART3 DMA+IDLE 接收驱动。
  *
  *          USART3 默认引脚: PB10(TX) / PB11(RX)
  *          波特率: 115200, 8N1
  *          DMA1_Channel3: 环形缓冲区接收
  *          IDLE 中断: 仅通知主循环，协议解析在 SlaveMCU_Process
  *
  *          数据包: 版本化定长帧，固定字节序，CRC16-CCITT 校验
  ******************************************************************************
  */

#include "SlaveMCU.h"
#include "dma_rx.h"
#include "inter_mcu_protocol.h"
#include <string.h>

/* ============================================
 * 协议常量
 * ============================================ */
#define SLAVE_BAUDRATE      115200u
#define SLAVE_DMA_BUF_SIZE  256u       /* DMA 环形缓冲区 */

/* ============================================
 * 全局从控传感器数据实例
 * ============================================ */
SlaveSensor_t slave;

/* ============================================
 * DMA 环形缓冲区
 * ============================================ */
static uint8_t slave_dma_buf[SLAVE_DMA_BUF_SIZE] __attribute__((aligned(4)));
static DmaRx slave_rx;
static uint8_t slave_frame[INTER_MCU_FRAME_SIZE];
static uint16_t slave_frame_length;
static uint8_t sequence_initialized;
static uint16_t expected_sequence;
#define SLAVE_SERVICE_BYTES 64u

/* ============================================
 * 静态函数声明
 * ============================================ */
static void Slave_GPIO_Config(void);
static void Slave_USART_Config(void);
static void Slave_DMA_Config(void);
static void Slave_ApplyPacket(const InterMcuSensorData *packet);
static void Slave_FeedByte(uint8_t byte);

/* ============================================
 * 初始化从控数据接收链路。
 * ============================================ */
void SlaveMCU_Init(void)
{
    memset(&slave, 0, sizeof(slave));
    slave_frame_length = 0u;
    sequence_initialized = 0u;
    expected_sequence = 0u;

    Slave_GPIO_Config();
    Slave_USART_Config();
    Slave_DMA_Config();
}

/* ============================================
 * GPIO: USART3 默认引脚 PB10(TX) / PB11(RX)。
 * ============================================ */
static void Slave_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* PB10 - USART3_TX (复用推挽) */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* PB11 - USART3_RX (浮空输入) */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/* ============================================
 * USART3: 115200, 8N1, RX+DMA, IDLE 中断
 * ============================================ */
static void Slave_USART_Config(void)
{
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    /* USART3 位于 APB1。 */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    USART_StructInit(&USART_InitStructure);
    USART_InitStructure.USART_BaudRate            = SLAVE_BAUDRATE;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &USART_InitStructure);

    /* 开启 DMA 接收 */
    USART_DMACmd(USART3, USART_DMAReq_Rx, ENABLE);

    /* 开启 IDLE 中断（总线空闲检测） */
    USART_ITConfig(USART3, USART_IT_IDLE, ENABLE);

    /* 优先级低于姿态控制相关定时器，避免影响飞控时序。 */
    NVIC_InitStructure.NVIC_IRQChannel                   = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART3, ENABLE);
}

/* ============================================
 * DMA1_Channel3: 环形模式，USART3_RX → 内存
 * ============================================ */
static void Slave_DMA_Config(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA1_Channel3);

    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(USART3->DR);
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)slave_dma_buf;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize         = SLAVE_DMA_BUF_SIZE;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel3, &DMA_InitStructure);

    DmaRx_Init(&slave_rx, DMA1_Channel3, slave_dma_buf,
               SLAVE_DMA_BUF_SIZE, DMA1_FLAG_HT3, DMA1_FLAG_TC3);
    DMA_ITConfig(DMA1_Channel3, DMA_IT_HT | DMA_IT_TC, ENABLE);
    DMA_Cmd(DMA1_Channel3, ENABLE);
}

/* ============================================
 * USART3 中断服务函数
 * IDLE 中断仅清硬件标志并发布接收事件。
 * ============================================ */
void USART3_IRQHandler(void)
{
    if (USART_GetITStatus(USART3, USART_IT_IDLE) != RESET)
    {
        /* 清除 IDLE 标志：先读 SR，再读 DR */
        volatile uint32_t tmp = USART3->SR;
        tmp = USART3->DR;
        (void)tmp;

        DmaRx_NotifyFromIsr(&slave_rx);
    }
}

void DMA1_Channel3_IRQHandler(void)
{
    DmaRx_OnDmaInterrupt(&slave_rx);
}

/* One bounded batch per main-loop turn. 41 bytes at 20Hz/115200 takes
 * 3.56ms on the wire; the 256-byte ring retains up to six complete frames.
 * IDLE is a notification, not a protocol frame delimiter.
 */
void SlaveMCU_Process(void)
{
    uint8_t bytes[SLAVE_SERVICE_BYTES];
    uint8_t dropped;
    uint16_t index;
    uint16_t count = DmaRx_Read(&slave_rx, bytes, sizeof(bytes), &dropped);

    if (dropped != 0u) slave_frame_length = 0u;
    slave.rx_overruns = slave_rx.overruns;
    for (index = 0u; index < count; ++index) {
        Slave_FeedByte(bytes[index]);
    }
}

/* Sliding frame search preserves partial frames and recovers from noise,
 * bad CRCs, and a frame header embedded in a rejected candidate.
 */
static void Slave_FeedByte(uint8_t byte)
{
    InterMcuSensorData packet;
    InterMcuDecodeStatus status;

    slave_frame[slave_frame_length++] = byte;
    while (slave_frame_length != 0u) {
        if (slave_frame[0] != INTER_MCU_MAGIC_0 ||
            (slave_frame_length >= 2u && slave_frame[1] != INTER_MCU_MAGIC_1)) {
            --slave_frame_length;
            memmove(slave_frame, slave_frame + 1, slave_frame_length);
            continue;
        }
        if (slave_frame_length < INTER_MCU_FRAME_SIZE) return;
        status = InterMcu_DecodeSensorFrame(slave_frame, INTER_MCU_FRAME_SIZE, &packet);
        if (status == INTER_MCU_DECODE_OK) {
            Slave_ApplyPacket(&packet);
            slave_frame_length = 0u;
            return;
        }
        if (status == INTER_MCU_DECODE_CRC) slave.crc_errors++;
        else slave.format_errors++;
        --slave_frame_length;
        memmove(slave_frame, slave_frame + 1, slave_frame_length);
    }
}

/* Main-loop publication: no floating point or protocol work in an ISR. */
/* ============================================
 * 将已经通过协议校验的数据原子地发布给主循环。
 * ============================================ */
static void Slave_ApplyPacket(const InterMcuSensorData *packet)
{
    if ((sequence_initialized != 0u) &&
        (packet->sequence != expected_sequence))
    {
        slave.sequence_gaps++;
    }
    sequence_initialized = 1u;
    expected_sequence = (uint16_t)(packet->sequence + 1u);

    slave.baro_altitude = (float)packet->baro_altitude_mm / 10.0f;
    slave.mag_yaw = (float)packet->yaw_centi_deg / 100.0f;
    slave.flow_x = packet->flow_x;
    slave.flow_y = packet->flow_y;
    slave.flow_distance = packet->flow_distance_mm;
    slave.flow_quality = packet->flow_quality;
    slave.flow_altitude = (float)packet->flow_distance_mm / 10.0f;
    slave.status_flags = packet->flags;
    slave.sequence = packet->sequence;
    slave.timestamp_ms = packet->timestamp_ms;
    slave.pressure_pa = packet->pressure_pa;
    slave.temperature_centi_c = packet->temperature_centi_c;
    slave.battery_mv = packet->battery_mv;
    slave.frames_received++;
    slave.updated = 1;
}
