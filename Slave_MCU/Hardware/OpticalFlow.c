#include "OpticalFlow.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "string.h"

/* 光流模块解析结果与接收状态。 */
OpticalFlow_Data_t OpticalFlow_Data;
volatile uint8_t OpticalFlow_RxFlag = 0;

static uint8_t  rx_buffer[20];
static uint8_t  rx_index = 0;
static uint8_t  total_len = 0;
static volatile uint16_t rx_timeout = 0;

/* 光流模块协议常量，依据实际串口数据格式整理。 */
#define FRAME_HEADER       0x24
#define MSG_TYPE_DISTANCE  0x01   /* 14 字节测距包 */
#define MSG_TYPE_FLOW      0x02   /* 18 字节光流包 */
#define DATA_VALID_FLAG    0xF5

/* 初始化 USART1：PA9-TX，PA10-RX，115200，用于接收光流模块数据。 */
void OpticalFlow_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef  NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &USART_InitStructure);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel                   = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART1, ENABLE);

    memset(&OpticalFlow_Data, 0, sizeof(OpticalFlow_Data));
    OpticalFlow_RxFlag = 0;
    rx_index  = 0;
    rx_timeout = 0;
}

/* 解析光流模块数据包。
 *
 * 通用格式：
 * [0]    帧头 0x24
 * [1:2]  模块 ID 0x58 0x3C
 * [4]    消息类型：0x01=测距，0x02=光流
 * [6]    负载长度：0x05 或 0x09
 * [8..]  负载数据
 *
 * 测距包：信号强度、距离(mm)、固件版本。
 * 光流包：有效标志 0xF5、flow_x、flow_y，小端 int32。
 */
static void ParsePacket(const uint8_t *buf, uint8_t len)
{
    /* 基本帧头校验。 */
    if (buf[0] != 0x24 || buf[1] != 0x58 || buf[2] != 0x3C) return;

    uint8_t payload_len = buf[6];
    if (len != (uint8_t)(8 + payload_len + 1)) return;

    uint8_t msg_type = buf[4];

    if (msg_type == MSG_TYPE_FLOW && payload_len == 0x09) {
        /* 光流包。 */
        if (buf[8] == DATA_VALID_FLAG) {
            OpticalFlow_Data.flow_x = (int32_t)(
                ((uint32_t)buf[12] << 24) |
                ((uint32_t)buf[11] << 16) |
                ((uint32_t)buf[10] << 8)  |
                ((uint32_t)buf[9])
            );
            OpticalFlow_Data.flow_y = (int32_t)(
                ((uint32_t)buf[16] << 24) |
                ((uint32_t)buf[15] << 16) |
                ((uint32_t)buf[14] << 8)  |
                ((uint32_t)buf[13])
            );
            OpticalFlow_Data.data_valid = 1;
        } else {
            OpticalFlow_Data.flow_x = 0;
            OpticalFlow_Data.flow_y = 0;
            OpticalFlow_Data.data_valid = 0;
        }
        OpticalFlow_RxFlag = 1;
    }
    else if (msg_type == MSG_TYPE_DISTANCE && payload_len == 0x05) {
        /* 测距包。 */
        OpticalFlow_Data.signal_strength  = buf[8];
        OpticalFlow_Data.distance         = (uint16_t)((buf[10] << 8) | buf[9]);
        OpticalFlow_Data.firmware_version = buf[11];
        OpticalFlow_RxFlag = 1;
    }
}

/* USART1 接收中断：按状态机接收光流变长帧。 */
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint8_t rx = (uint8_t)USART_ReceiveData(USART1);
        rx_timeout = 0;

        if (rx_index == 0) {
            /* 等待帧头。 */
            if (rx == FRAME_HEADER) {
                rx_buffer[0] = rx;
                rx_index = 1;
            }
        }
        else if (rx_index < 7) {
            /* 读取包头 [1]~[6]；如果遇到新帧头则重新同步。 */
            if (rx == FRAME_HEADER) {
                rx_buffer[0] = rx;
                rx_index = 1;
                USART_ClearITPendingBit(USART1, USART_IT_RXNE);
                return;
            }
            rx_buffer[rx_index++] = rx;

            /* 收到 payload_len 后确定总包长。 */
            if (rx_index == 7) {
                uint8_t pl = rx_buffer[6];
                total_len = 8 + pl + 1;
                if (total_len > 20 || total_len < 9) {
                    rx_index = 0;
                }
            }
        }
        else {
            /* 读取负载和校验和。 */
            rx_buffer[rx_index++] = rx;

            if (rx_index >= total_len) {
                ParsePacket(rx_buffer, total_len);
                rx_index = 0;
            }
        }

        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}

/* 半帧超时检测，由主循环周期调用。 */
void OpticalFlow_TimeoutCheck(void)
{
    if (rx_index > 0) {
        rx_timeout++;
        if (rx_timeout > 50) {
            rx_index   = 0;
            rx_timeout = 0;
        }
    }
}

/* 查询是否收到新数据，读取后自动清标志。 */
uint8_t OpticalFlow_HasNewData(void)
{
    if (OpticalFlow_RxFlag) {
        OpticalFlow_RxFlag = 0;
        return 1;
    }
    return 0;
}

uint8_t IsDataValid(void)
{
    return OpticalFlow_Data.data_valid;
}

int32_t GetFlowX(void)
{
    return OpticalFlow_Data.flow_x;
}

int32_t GetFlowY(void)
{
    return OpticalFlow_Data.flow_y;
}

uint16_t GetDistance(void)
{
    return OpticalFlow_Data.distance;
}

uint8_t GetSignalStrength(void)
{
    return OpticalFlow_Data.signal_strength;
}
