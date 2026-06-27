#include "OpticalFlow.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "string.h"

// ============================================================
// 全局变量
// ============================================================
OpticalFlow_Data_t OpticalFlow_Data;
volatile uint8_t OpticalFlow_RxFlag = 0;

static uint8_t  rx_buffer[20];
static uint8_t  rx_index = 0;
static uint8_t  total_len = 0;
static volatile uint16_t rx_timeout = 0;

// ============================================================
// 协议常量（从实际数据逆向得出）
// ============================================================
#define FRAME_HEADER       0x24
#define MSG_TYPE_DISTANCE  0x01   // 14字节，测距包
#define MSG_TYPE_FLOW      0x02   // 18字节，光流包
#define DATA_VALID_FLAG    0xF5

/**
 * @brief  串口初始化（USART1, PA9-Tx, PA10-Rx, 115200）
 */
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

/**
 * @brief  解析数据包
 *
 *  实际协议结构：
 *  [0]      帧头 0x24
 *  [1][2]   模块ID 0x58 0x3C
 *  [3]      保留 0x00
 *  [4]      消息类型（0x01=测距，0x02=光流）
 *  [5]      参数/计数
 *  [6]      负载长度（0x05或0x09）
 *  [7]      保留 0x00
 *  [8..]    负载数据
 *  [last]   校验和
 *
 *  测距包（14字节，payload_len=5）：
 *  [8]      信号强度 0~100
 *  [9][10]  距离，[9]=低8位，[10]=高8位，单位mm
 *  [11]     固件版本
 *  [12]     保留
 *  [13]     校验和
 *
 *  光流包（18字节，payload_len=9）：
 *  [8]      数据有效标志（0xF5=有效）
 *  [9~12]   光流X，int32小端，原始值
 *  [13~16]  光流Y，int32小端，原始值
 *  [17]     校验和
 */
static void ParsePacket(const uint8_t *buf, uint8_t len)
{
    // 基本校验
    if (buf[0] != 0x24 || buf[1] != 0x58 || buf[2] != 0x3C) return;

    uint8_t payload_len = buf[6];
    if (len != (uint8_t)(8 + payload_len + 1)) return;

    uint8_t msg_type = buf[4];

    if (msg_type == MSG_TYPE_FLOW && payload_len == 0x09) {
        // ---- 光流包 ----
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
        // ---- 测距包 ----
        OpticalFlow_Data.signal_strength  = buf[8];
        OpticalFlow_Data.distance         = (uint16_t)((buf[10] << 8) | buf[9]);
        OpticalFlow_Data.firmware_version = buf[11];
        OpticalFlow_RxFlag = 1;
    }
}

/**
 * @brief  USART1中断（状态机接收，支持变长包）
 */
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint8_t rx = (uint8_t)USART_ReceiveData(USART1);
        rx_timeout = 0;

        if (rx_index == 0) {
            // 等待帧头
            if (rx == FRAME_HEADER) {
                rx_buffer[0] = rx;
                rx_index = 1;
            }
        }
        else if (rx_index < 7) {
            // 读取包头 [1]~[6]，此阶段遇到新帧头则重新同步
            if (rx == FRAME_HEADER) {
                rx_buffer[0] = rx;
                rx_index = 1;
                USART_ClearITPendingBit(USART1, USART_IT_RXNE);
                return;
            }
            rx_buffer[rx_index++] = rx;

            // 收到 [6] 后确定总包长
            if (rx_index == 7) {
                uint8_t pl = rx_buffer[6];
                total_len = 8 + pl + 1;
                if (total_len > 20 || total_len < 9) {
                    rx_index = 0;  // 长度非法，重置
                }
            }
        }
        else {
            // 读取负载 + 校验和
            rx_buffer[rx_index++] = rx;

            if (rx_index >= total_len) {
                ParsePacket(rx_buffer, total_len);
                rx_index = 0;
            }
        }

        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}

/**
 * @brief  半帧超时检测（1ms调用一次）
 */
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

/**
 * @brief  查询新数据（读取后自动清标志）
 */
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
