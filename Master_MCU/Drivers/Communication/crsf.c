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
int16_t rc_channels[16] = {0};
volatile uint8_t crsf_frame_received = 0;
volatile uint32_t crsf_valid_frame_count = 0;
volatile uint32_t crsf_crc_error_count = 0;

/* DMA 环形接收缓冲 */
static uint8_t  crsf_dma_buf[CRSF_DMA_BUF_SIZE] __attribute__((aligned(4)));
static uint32_t crsf_dma_last_ndtr = 0;

/* CRSF 帧解析状态 */
static uint8_t  crsf_frame_buf[CRSF_MAX_FRAME_LEN + 4]; /* sync + len + type + payload + crc */
static uint8_t  crsf_frame_ofs = 0;
static uint8_t  crsf_expected_len = 0;
static uint8_t  crsf_in_frame = 0;

/* 静态函数声明 */
static void crsf_gpio_config(void);
static void crsf_usart_config(void);
static void crsf_dma_config(void);
static void crsf_parse_frame(const uint8_t *frame, uint8_t len);
static void crsf_unpack_rc(const uint8_t *payload);
static void crsf_feed_byte(uint8_t byte);
static void crsf_read_dma_buffer(void);
static uint8_t crsf_crc8_dvb_s2(const uint8_t *data, uint8_t len);

/* 初始化 CRSF 接收链路。 */
void crsf_init(void)
{
    memset(rc_channels, 0, sizeof(rc_channels));
    crsf_frame_received = 0;
    crsf_valid_frame_count = 0;
    crsf_crc_error_count = 0;

    /* 安全默认值：横滚/俯仰/偏航居中，油门最低。 */
    rc_channels[0] = 1500;
    rc_channels[1] = 1500;
    rc_channels[2] = 1000;
    rc_channels[3] = 1500;

    crsf_gpio_config();
    crsf_usart_config();
    crsf_dma_config();
}

/* GPIO 配置：PA2=USART2_TX，PA3=USART2_RX。 */
static void crsf_gpio_config(void)
{
    GPIO_InitTypeDef gpio_config;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* PA2：复用推挽输出。 */
    gpio_config.GPIO_Pin   = GPIO_Pin_2;
    gpio_config.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_config.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio_config);

    /* PA3：浮空输入。 */
    gpio_config.GPIO_Pin  = GPIO_Pin_3;
    gpio_config.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio_config);
}

/* USART2 配置：420000 baud，8N1。 */
static void crsf_usart_config(void)
{
    USART_InitTypeDef usart_config;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    USART_StructInit(&usart_config);
    usart_config.USART_BaudRate            = CRSF_BAUDRATE;
    usart_config.USART_WordLength          = USART_WordLength_8b;
    usart_config.USART_StopBits            = USART_StopBits_1;
    usart_config.USART_Parity              = USART_Parity_No;
    usart_config.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_config.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &usart_config);

    /* USART2_RX 使用 DMA1_Channel6 接收。 */
    USART_DMACmd(USART2, USART_DMAReq_Rx, ENABLE);

    USART_Cmd(USART2, ENABLE);
}

/* DMA1_Channel6 配置为环形接收。 */
static void crsf_dma_config(void)
{
    DMA_InitTypeDef dma_config;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA1_Channel6);

    dma_config.DMA_PeripheralBaseAddr = (uint32_t)&(USART2->DR);
    dma_config.DMA_MemoryBaseAddr     = (uint32_t)crsf_dma_buf;
    dma_config.DMA_DIR                = DMA_DIR_PeripheralSRC;
    dma_config.DMA_BufferSize         = CRSF_DMA_BUF_SIZE;
    dma_config.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    dma_config.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    dma_config.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma_config.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    dma_config.DMA_Mode               = DMA_Mode_Circular;
    dma_config.DMA_Priority           = DMA_Priority_High;
    dma_config.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel6, &dma_config);

    DMA_Cmd(DMA1_Channel6, ENABLE);

    /* 记录初始 NDTR，后续通过差值计算新增字节数。 */
    crsf_dma_last_ndtr = DMA_GetCurrDataCounter(DMA1_Channel6);
}

/* 主循环周期调用：从 DMA 环形缓冲取出新字节并喂给帧状态机。 */
void crsf_process(void)
{
    crsf_read_dma_buffer();
}

/* 根据 DMA NDTR 变化读取新增字节。 */
static void crsf_read_dma_buffer(void)
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
            crsf_feed_byte(byte);
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
static void crsf_feed_byte(uint8_t byte)
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
        if (crsf_expected_len < 2u || crsf_expected_len > CRSF_MAX_FRAME_LEN)
        {
            /* 长度非法，放弃当前帧。 */
            crsf_in_frame = 0;
            crsf_frame_ofs = 0;
        }
    }

    if (crsf_in_frame && crsf_frame_ofs >= (uint8_t)(crsf_expected_len + 2))
    {
        /* 帧完整：sync(1) + len(1) + payload(len)。 */
        crsf_parse_frame(crsf_frame_buf, crsf_frame_ofs);
        crsf_in_frame = 0;
        crsf_frame_ofs = 0;
    }
}

/* 解析完整 CRSF 帧，格式：[sync][len][type][payload...][crc]。 */
static void crsf_parse_frame(const uint8_t *frame, uint8_t len)
{
    uint8_t calculated_crc;
    uint8_t received_crc;
    uint8_t type;

    if (len < 4u) return; /* 最小帧：sync + len + type + crc */
    if (len != (uint8_t)(frame[1] + 2u)) return;

    /* CRSF CRC覆盖type和payload，不包含sync、length和末尾CRC。 */
    received_crc = frame[len - 1u];
    calculated_crc = crsf_crc8_dvb_s2(&frame[2], (uint8_t)(len - 3u));
    if (calculated_crc != received_crc)
    {
        crsf_crc_error_count++;
        return;
    }

    type = frame[2];

    switch (type)
    {
    case CRSF_TYPE_RC_CHANNELS:
        /* RC 通道 payload 为 22 字节：16 通道 * 11 bit。 */
        if (frame[1] == (CRSF_RC_CHANNELS_PAYLOAD + 2u) &&
            len == (CRSF_RC_CHANNELS_PAYLOAD + 4u))
        {
            crsf_unpack_rc(&frame[3]);
            crsf_valid_frame_count++;
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

/* CRSF标准CRC8 DVB-S2，Polynomial = 0xD5。 */
static uint8_t crsf_crc8_dvb_s2(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    uint8_t bit;

    while (len--)
    {
        crc ^= *data++;
        for (bit = 0; bit < 8u; bit++)
        {
            if (crc & 0x80u)
            {
                crc = (uint8_t)((crc << 1) ^ 0xD5u);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

/* 解包 16 个 RC 通道。
 * CRSF 使用 22 字节承载 16 个 11bit 通道，LSB-first 打包。
 * 原始范围约 172~1811，对应输出 1000~2000us。
 */
static void crsf_unpack_rc(const uint8_t *payload)
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
            rc_channels[i] = (int16_t)CRSF_RC_OUT_MIN;
        }
        else if (raw[i] >= CRSF_RC_CH_MAX)
        {
            rc_channels[i] = (int16_t)CRSF_RC_OUT_MAX;
        }
        else
        {
            /* 线性插值。 */
            rc_channels[i] = (int16_t)(
                CRSF_RC_OUT_MIN +
                ((int32_t)(raw[i] - CRSF_RC_CH_MIN) *
                 (int32_t)(CRSF_RC_OUT_MAX - CRSF_RC_OUT_MIN)) /
                (int32_t)(CRSF_RC_CH_MAX - CRSF_RC_CH_MIN)
            );
        }
    }
}
