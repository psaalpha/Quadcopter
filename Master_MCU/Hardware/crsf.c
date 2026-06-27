/**
  ******************************************************************************
  * @file    crsf.c
  * @brief   CRSF/ExpressLRS 遥控协议解析，基于 USART2 + DMA 环形缓冲。
  *
  *          USART2: PA2(TX) / PA3(RX)
  *          波特率: 420000, 8N1
  *          DMA1_Channel6: USART2_RX 环形接收
  ******************************************************************************
  */

#include "crsf.h"
#include <string.h>

/* 协议和串口配置常量 */
#define CRSF_BAUDRATE   420000u

/* 遥控通道输出，单位映射到 1000~2000us */
int16_t rcChannels[16] = {0};
volatile uint8_t crsf_frame_received = 0;

/* DMA 环形接收缓冲 */
static uint8_t  crsf_dma_buf[CRSF_DMA_BUF_SIZE] __attribute__((aligned(4)));
static uint32_t crsf_dma_last_ndtr = 0;

/* CRSF 帧解析状态 */
static uint8_t  crsf_frame_buf[CRSF_MAX_FRAME_LEN + 4]; /* sync + len + type + payload + crc */
static uint8_t  crsf_frame_ofs = 0;
static uint8_t  crsf_expected_len = 0;
static uint8_t  crsf_in_frame = 0;

/* 静态函数声明 */
static void CRSF_GPIO_Config(void);
static void CRSF_USART_Config(void);
static void CRSF_DMA_Config(void);
static void CRSF_ParseFrame(const uint8_t *frame, uint8_t len);
static void CRSF_UnpackRC(const uint8_t *payload);
static void CRSF_FeedByte(uint8_t byte);
static void CRSF_ReadDMABuffer(void);

/* 初始化 CRSF 接收链路。 */
void CRSF_Init(void)
{
    memset(rcChannels, 0, sizeof(rcChannels));

    /* 安全默认值：横滚/俯仰/偏航居中，油门最低。 */
    rcChannels[0] = 1500;
    rcChannels[1] = 1500;
    rcChannels[2] = 1000;
    rcChannels[3] = 1500;

    CRSF_GPIO_Config();
    CRSF_USART_Config();
    CRSF_DMA_Config();
}

/* GPIO 配置：PA2=USART2_TX，PA3=USART2_RX。 */
static void CRSF_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* PA2：复用推挽输出。 */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA3：浮空输入。 */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/* USART2 配置：420000 baud，8N1。 */
static void CRSF_USART_Config(void)
{
    USART_InitTypeDef USART_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    USART_StructInit(&USART_InitStructure);
    USART_InitStructure.USART_BaudRate            = CRSF_BAUDRATE;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);

    /* USART2_RX 使用 DMA1_Channel6 接收。 */
    USART_DMACmd(USART2, USART_DMAReq_Rx, ENABLE);

    USART_Cmd(USART2, ENABLE);
}

/* DMA1_Channel6 配置为环形接收。 */
static void CRSF_DMA_Config(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA1_Channel6);

    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(USART2->DR);
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)crsf_dma_buf;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize         = CRSF_DMA_BUF_SIZE;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel6, &DMA_InitStructure);

    DMA_Cmd(DMA1_Channel6, ENABLE);

    /* 记录初始 NDTR，后续通过差值计算新增字节数。 */
    crsf_dma_last_ndtr = DMA_GetCurrDataCounter(DMA1_Channel6);
}

/* 主循环周期调用：从 DMA 环形缓冲取出新字节并喂给帧状态机。 */
void CRSF_Process(void)
{
    CRSF_ReadDMABuffer();
}

/* 根据 DMA NDTR 变化读取新增字节。 */
static void CRSF_ReadDMABuffer(void)
{
    uint32_t ndtr = DMA_GetCurrDataCounter(DMA1_Channel6);

    /* NDTR 递减计数，通过上一次 NDTR 与当前 NDTR 的差值计算新增字节。 */
    uint32_t new_bytes;
    if (ndtr <= crsf_dma_last_ndtr)
    {
        /* 未回绕。 */
        new_bytes = crsf_dma_last_ndtr - ndtr;
    }
    else
    {
        /* 环形缓冲发生回绕。 */
        new_bytes = CRSF_DMA_BUF_SIZE - ndtr + crsf_dma_last_ndtr;
    }

    if (new_bytes > 0 && new_bytes < CRSF_DMA_BUF_SIZE)
    {
        uint32_t start_idx = (CRSF_DMA_BUF_SIZE - crsf_dma_last_ndtr) % CRSF_DMA_BUF_SIZE;
        for (uint32_t i = 0; i < new_bytes; i++)
        {
            uint8_t byte = crsf_dma_buf[(start_idx + i) % CRSF_DMA_BUF_SIZE];
            CRSF_FeedByte(byte);
        }

        crsf_dma_last_ndtr = ndtr;
    }
    /* 异常或溢出时重置游标。 */
    else if (new_bytes >= CRSF_DMA_BUF_SIZE)
    {
        crsf_dma_last_ndtr = ndtr;
    }
}

/* 单字节喂入 CRSF 帧状态机：SYNC -> LEN -> TYPE/PAYLOAD/CRC。 */
static void CRSF_FeedByte(uint8_t byte)
{
    if (!crsf_in_frame)
    {
        /* 查找同步字节。 */
        if (byte == CRSF_SYNC_BYTE_RC || byte == CRSF_SYNC_BYTE_TLM)
        {
            crsf_frame_buf[0] = byte;
            crsf_frame_ofs    = 1;
            crsf_in_frame     = 1;
            crsf_expected_len = 0;
        }
        return;
    }

    /* 保存当前帧字节。 */
    if (crsf_frame_ofs < sizeof(crsf_frame_buf))
    {
        crsf_frame_buf[crsf_frame_ofs] = byte;
    }
    crsf_frame_ofs++;

    if (crsf_frame_ofs == 2)
    {
        /* 长度字节包含 type + payload + crc。 */
        crsf_expected_len = byte;
        if (crsf_expected_len > CRSF_MAX_FRAME_LEN)
        {
            /* 长度非法，放弃当前帧。 */
            crsf_in_frame = 0;
            crsf_frame_ofs = 0;
        }
    }

    if (crsf_in_frame && crsf_frame_ofs >= (uint8_t)(crsf_expected_len + 2))
    {
        /* 帧完整：sync(1) + len(1) + payload(len)。 */
        CRSF_ParseFrame(crsf_frame_buf, crsf_frame_ofs);
        crsf_in_frame = 0;
        crsf_frame_ofs = 0;
    }
}

/* 解析完整 CRSF 帧，格式：[sync][len][type][payload...][crc]。 */
static void CRSF_ParseFrame(const uint8_t *frame, uint8_t len)
{
    if (len < 4) return; /* 最小帧：sync + len + type + crc */

    uint8_t type = frame[2];

    switch (type)
    {
    case CRSF_TYPE_RC_CHANNELS:
        /* RC 通道 payload 为 22 字节：16 通道 * 11 bit。 */
        if (len >= 26) /* sync(1) + len(1) + type(1) + 22 + crc(1) */
        {
            CRSF_UnpackRC(&frame[3]);
            crsf_frame_received = 1;
        }
        break;

    case CRSF_TYPE_LINK_STATS:
    case CRSF_TYPE_ATTITUDE:
    case CRSF_TYPE_BATTERY:
    case CRSF_TYPE_GPS:
    case CRSF_TYPE_HEARTBEAT:
    default:
        /* 其他帧类型当前不处理。 */
        break;
    }
}

/* 解包 16 个 RC 通道。
 * CRSF 使用 22 字节承载 16 个 11bit 通道，LSB-first 打包。
 * 原始范围约 172~1811，对应输出 1000~2000us。
 */
static void CRSF_UnpackRC(const uint8_t *payload)
{
    uint16_t raw[16];
    uint8_t  bits_merged  = 0;
    uint32_t read_value   = 0;
    uint8_t  byte_idx     = 0;

    for (int i = 0; i < 16; i++)
    {
        /* 累积至少 11bit 后取出一个通道值。 */
        while (bits_merged < 11)
        {
            read_value |= ((uint32_t)payload[byte_idx]) << bits_merged;
            byte_idx++;
            bits_merged += 8;
        }

        /* 取低 11bit。 */
        raw[i] = (uint16_t)(read_value & 0x07FFu);

        /* 消耗 11bit，继续解析下一通道。 */
        read_value >>= 11;
        bits_merged  -= 11;
    }

    /* 映射到常用 PWM 通道范围 1000~2000us。 */
    for (int i = 0; i < 16; i++)
    {
        if (raw[i] <= CRSF_RC_CH_MIN)
        {
            rcChannels[i] = (int16_t)CRSF_RC_OUT_MIN;
        }
        else if (raw[i] >= CRSF_RC_CH_MAX)
        {
            rcChannels[i] = (int16_t)CRSF_RC_OUT_MAX;
        }
        else
        {
            /* 线性插值。 */
            rcChannels[i] = (int16_t)(
                CRSF_RC_OUT_MIN +
                ((int32_t)(raw[i] - CRSF_RC_CH_MIN) *
                 (int32_t)(CRSF_RC_OUT_MAX - CRSF_RC_OUT_MIN)) /
                (int32_t)(CRSF_RC_CH_MAX - CRSF_RC_CH_MIN)
            );
        }
    }
}
