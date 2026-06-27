/**
  ******************************************************************************
  * @file    crsf.c
  * @brief   CRSF (ExpressLRS) RC protocol parser using USART2 + DMA
  *
  *          USART2: PA2(TX) / PA3(RX)
  *          Baudrate: 420000, 8N1
  *          DMA1 Channel 6: circular-mode RX into ring buffer
  ******************************************************************************
  */

#include "crsf.h"
#include <string.h>

/* ============================================
 * Local Constants
 * ============================================ */
#define CRSF_BAUDRATE   420000u

/* ============================================
 * Global Variables
 * ============================================ */
int16_t rcChannels[16] = {0};
volatile uint8_t crsf_frame_received = 0;

/* ============================================
 * DMA Circular Buffer
 * ============================================ */
static uint8_t  crsf_dma_buf[CRSF_DMA_BUF_SIZE] __attribute__((aligned(4)));
static uint32_t crsf_dma_last_ndtr = 0;

/* ============================================
 * Internal State
 * ============================================ */
static uint8_t  crsf_frame_buf[CRSF_MAX_FRAME_LEN + 4]; /* sync + len + type + payload + crc */
static uint8_t  crsf_frame_ofs = 0;
static uint8_t  crsf_expected_len = 0;
static uint8_t  crsf_in_frame = 0;

/* ============================================
 * Local Function Prototypes
 * ============================================ */
static void CRSF_GPIO_Config(void);
static void CRSF_USART_Config(void);
static void CRSF_DMA_Config(void);
static void CRSF_ParseFrame(const uint8_t *frame, uint8_t len);
static void CRSF_UnpackRC(const uint8_t *payload);
static void CRSF_FeedByte(uint8_t byte);
static void CRSF_ReadDMABuffer(void);

/* ============================================
 * CRSF_Init - Initialize USART2 + DMA for CRSF
 * ============================================ */
void CRSF_Init(void)
{
    /* Zero out channel array */
    memset(rcChannels, 0, sizeof(rcChannels));

    /* Default mid-stick for safety */
    rcChannels[0] = 1500;
    rcChannels[1] = 1500;
    rcChannels[2] = 1000;
    rcChannels[3] = 1500;

    CRSF_GPIO_Config();
    CRSF_USART_Config();
    CRSF_DMA_Config();
}

/* ============================================
 * GPIO Configuration: PA2=USART2_TX (AF PP)
 *                     PA3=USART2_RX (Input floating)
 * ============================================ */
static void CRSF_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* Enable GPIOA clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* PA2 - USART2 TX (Alternate function push-pull) */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA3 - USART2 RX (Input floating) */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/* ============================================
 * USART2 Configuration: 420000 baud, 8N1
 * USART2 is on APB1 bus (max 36MHz)
 * ============================================ */
static void CRSF_USART_Config(void)
{
    USART_InitTypeDef USART_InitStructure;

    /* Enable USART2 clock (APB1) */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    USART_StructInit(&USART_InitStructure);
    USART_InitStructure.USART_BaudRate            = CRSF_BAUDRATE;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);

    /* Enable DMA receiver - USART2_RX is on DMA1_Channel6 */
    USART_DMACmd(USART2, USART_DMAReq_Rx, ENABLE);

    /* Enable USART2 */
    USART_Cmd(USART2, ENABLE);
}

/* ============================================
 * DMA1 Channel 6 Configuration: Circular mode
 * USART2_RX is on DMA1_Channel6
 * ============================================ */
static void CRSF_DMA_Config(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    /* Enable DMA1 clock */
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

    /* Enable DMA channel */
    DMA_Cmd(DMA1_Channel6, ENABLE);

    /* Save initial NDTR value for tracking */
    crsf_dma_last_ndtr = DMA_GetCurrDataCounter(DMA1_Channel6);
}

/* ============================================
 * CRSF_Process - Called from main loop
 * Reads new data from DMA ring buffer and
 * feeds bytes to the CRSF frame parser
 * ============================================ */
void CRSF_Process(void)
{
    CRSF_ReadDMABuffer();
}

/* ============================================
 * Read new bytes from the DMA circular buffer
 * by tracking NDTR register changes
 * ============================================ */
static void CRSF_ReadDMABuffer(void)
{
    uint32_t ndtr = DMA_GetCurrDataCounter(DMA1_Channel6);

    /* Calculate how many bytes have been written by DMA */
    /* NDTR counts down from BUF_SIZE-1 to 0 */
    uint32_t new_bytes;
    if (ndtr <= crsf_dma_last_ndtr)
    {
        /* Normal case: DMA continued writing forward */
        new_bytes = crsf_dma_last_ndtr - ndtr;
    }
    else
    {
        /* Wrap-around: DMA wrapped back to start of buffer */
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
    /* Reset tracking if counter seems inconsistent (overflow recovery) */
    else if (new_bytes >= CRSF_DMA_BUF_SIZE)
    {
        crsf_dma_last_ndtr = ndtr;
    }
}

/* ============================================
 * Feed a single byte into the frame parser
 * Implements a simple state machine:
 *   IDLE -> (0xC8 or 0xEE) -> LEN -> TYPE -> PAYLOAD -> CRC
 * ============================================ */
static void CRSF_FeedByte(uint8_t byte)
{
    if (!crsf_in_frame)
    {
        /* Look for sync byte */
        if (byte == CRSF_SYNC_BYTE_RC || byte == CRSF_SYNC_BYTE_TLM)
        {
            crsf_frame_buf[0] = byte;
            crsf_frame_ofs    = 1;
            crsf_in_frame     = 1;
            crsf_expected_len = 0;
        }
        return;
    }

    /* Store byte in frame buffer */
    if (crsf_frame_ofs < sizeof(crsf_frame_buf))
    {
        crsf_frame_buf[crsf_frame_ofs] = byte;
    }
    crsf_frame_ofs++;

    if (crsf_frame_ofs == 2)
    {
        /* Length byte: payload length (type + data + crc) */
        crsf_expected_len = byte;
        if (crsf_expected_len > CRSF_MAX_FRAME_LEN)
        {
            /* Invalid length, abort */
            crsf_in_frame = 0;
            crsf_frame_ofs = 0;
        }
    }

    if (crsf_in_frame && crsf_frame_ofs >= (uint8_t)(crsf_expected_len + 2))
    {
        /* Frame complete: sync(1) + len(1) + payload(len) = 2+len bytes */
        CRSF_ParseFrame(crsf_frame_buf, crsf_frame_ofs);
        crsf_in_frame = 0;
        crsf_frame_ofs = 0;
    }
}

/* ============================================
 * Parse a complete CRSF frame
 * Format: [sync] [len] [type] [payload...] [crc]
 * ============================================ */
static void CRSF_ParseFrame(const uint8_t *frame, uint8_t len)
{
    if (len < 4) return; /* Minimum: sync + len + type + crc */

    uint8_t type = frame[2];

    switch (type)
    {
    case CRSF_TYPE_RC_CHANNELS:
        /* Payload is 22 bytes (16 channels × 11 bits) */
        if (len >= 26) /* sync(1) + len(1) + type(1) + 22 bytes + crc(1) */
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
        /* Other frame types: silently ignored */
        break;
    }
}

/* ============================================
 * Unpack 16 RC channels from 22-byte payload
 *
 * CRSF encodes channels as 11-bit values, LSB-first,
 * packed across 22 bytes (176 bits = 16 × 11)
 *
 * Standard CRSF bitstream (LSB-first within each byte):
 *   Byte[0] bits: Ch1[7:0]
 *   Byte[1] bits: Ch1[10:8] + Ch2[4:0]
 *   Byte[2] bits: Ch2[10:5] + Ch3[1:0]
 *   ...
 *
 * Raw range: 172 ~ 1811  (988us ~ 2012us)
 * Mapped to: 1000 ~ 2000us
 * ============================================ */
static void CRSF_UnpackRC(const uint8_t *payload)
{
    uint16_t raw[16];
    uint8_t  bits_merged  = 0;
    uint32_t read_value   = 0;
    uint8_t  byte_idx     = 0;

    for (int i = 0; i < 16; i++)
    {
        /* Accumulate at least 11 bits into read_value */
        while (bits_merged < 11)
        {
            read_value |= ((uint32_t)payload[byte_idx]) << bits_merged;
            byte_idx++;
            bits_merged += 8;
        }

        /* Extract 11 bits */
        raw[i] = (uint16_t)(read_value & 0x07FFu);

        /* Consume 11 bits for next channel */
        read_value >>= 11;
        bits_merged  -= 11;
    }

    /* Map raw CRSF range (172~1811) to output range (1000~2000us) */
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
            /* Linear interpolation */
            rcChannels[i] = (int16_t)(
                CRSF_RC_OUT_MIN +
                ((int32_t)(raw[i] - CRSF_RC_CH_MIN) *
                 (int32_t)(CRSF_RC_OUT_MAX - CRSF_RC_OUT_MIN)) /
                (int32_t)(CRSF_RC_CH_MAX - CRSF_RC_CH_MIN)
            );
        }
    }
}
