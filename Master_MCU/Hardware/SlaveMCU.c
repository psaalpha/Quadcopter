/**
  ******************************************************************************
  * @file    SlaveMCU.c
  * @brief   从机传感器 USART3 DMA+IDLE 接收驱动
  *
  *          USART3 部分重映射: PC10(TX) / PC11(RX)
  *          波特率: 115200, 8N1
  *          DMA1_Channel3: 环形缓冲区接收
  *          IDLE 中断: 检测帧结束，触发解析
  *
  *          数据包: 30 字节固定长度，帧头 0xA5，异或校验
  ******************************************************************************
  */

#include "SlaveMCU.h"
#include <string.h>

/* ============================================
 * 协议常量
 * ============================================ */
#define SLAVE_BAUDRATE      115200u
#define SLAVE_PACKET_LEN    30u        /* 固定包长 */
#define SLAVE_HEADER        0xA5u      /* 帧头 */
#define SLAVE_PAYLOAD_LEN   28u        /* length 字段期望值 */
#define SLAVE_DMA_BUF_SIZE  256u       /* DMA 环形缓冲区 */

/* ============================================
 * 全局传感器数据实例
 * ============================================ */
SlaveSensor_t slave;

/* ============================================
 * DMA 环形缓冲区
 * ============================================ */
static uint8_t  slave_dma_buf[SLAVE_DMA_BUF_SIZE] __attribute__((aligned(4)));

/* ============================================
 * 静态函数声明
 * ============================================ */
static void Slave_GPIO_Config(void);
static void Slave_USART_Config(void);
static void Slave_DMA_Config(void);
static void Slave_ParsePacket(const uint8_t *raw);
static void Slave_TryExtractFrame(void);

/* ============================================
 * SlaveMCU_Init
 * ============================================ */
void SlaveMCU_Init(void)
{
    memset(&slave, 0, sizeof(slave));

    Slave_GPIO_Config();
    Slave_USART_Config();
    Slave_DMA_Config();
}

/* ============================================
 * GPIO: USART3 默认引脚 PB10(TX) / PB11(RX)
 *       Key.c 已删除，PB11 无冲突，无需重映射
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

    /* 开启 USART3 时钟（APB1） */
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

    /* NVIC: 抢占2 响应2（低于 TIM2=1,1 和 TIM3=2,2，不干扰飞控时序） */
    NVIC_InitStructure.NVIC_IRQChannel                   = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;  /* 低于 TIM3(2,2)，不抢角度环 */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
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

    DMA_Cmd(DMA1_Channel3, ENABLE);
}

/* ============================================
 * USART3 中断服务函数
 * IDLE 中断 → 帧接收完毕 → 直接提取末尾 30 字节解析
 * 不再使用状态机，避免 payload 中 0xA5 导致误同步
 * ============================================ */
void USART3_IRQHandler(void)
{
    if (USART_GetITStatus(USART3, USART_IT_IDLE) != RESET)
    {
        /* 清除 IDLE 标志：先读 SR，再读 DR */
        volatile uint32_t tmp = USART3->SR;
        tmp = USART3->DR;
        (void)tmp;

        Slave_TryExtractFrame();

        /* 重置 DMA 到 buf[0]，避免长期运行游标错位 */
        DMA_Cmd(DMA1_Channel3, DISABLE);
        DMA_SetCurrDataCounter(DMA1_Channel3, SLAVE_DMA_BUF_SIZE);
        DMA_Cmd(DMA1_Channel3, ENABLE);
    }
}

/* ============================================
 * 从 DMA 环形缓冲区直接提取最后一帧（30 字节）
 * IDLE 中断保证帧已完整接收，无需状态机
 * ============================================ */
static void Slave_TryExtractFrame(void)
{
    uint32_t ndtr     = DMA_GetCurrDataCounter(DMA1_Channel3);
    uint32_t received = SLAVE_DMA_BUF_SIZE - ndtr;

    /* 至少收到一帧才处理 */
    if (received < SLAVE_PACKET_LEN)
        return;

    /* 最新一帧在缓冲区末尾 30 字节 */
    uint8_t  frame[SLAVE_PACKET_LEN];
    uint32_t start_ofs = received - SLAVE_PACKET_LEN;

    for (uint32_t i = 0; i < SLAVE_PACKET_LEN; i++)
    {
        frame[i] = slave_dma_buf[(start_ofs + i) % SLAVE_DMA_BUF_SIZE];
    }

    /* 帧头校验 */
    if (frame[0] != SLAVE_HEADER)
        return;

    /* 长度校验 */
    if (frame[1] != SLAVE_PAYLOAD_LEN)
        return;

    /* 异或校验：byte 0~28 */
    uint8_t csum = 0;
    for (uint8_t i = 0; i < 29; i++)
        csum ^= frame[i];

    if (csum == frame[29])
    {
        Slave_ParsePacket(frame);
    }
}

/* ============================================
 * 解析有效包，提取 5 个传感器字段
 * 包格式（小端序）：
 *   [0]=0xA5 [1]=28 [2-5]=pressure [6-9]=temp
 *   [10-13]=altitude [14-17]=yaw [18-21]=flow_x
 *   [22-25]=flow_y [26-27]=flow_distance [28]=quality [29]=csum
 * ============================================ */
static void Slave_ParsePacket(const uint8_t *raw)
{
    float    altitude;
    float    yaw;
    int32_t  flow_x;
    int32_t  flow_y;
    uint16_t flow_distance;
    uint8_t  flow_quality;

    /* 小端序读取各字段 */
    altitude      = *(float *)(&raw[10]);
    yaw           = *(float *)(&raw[14]);
    flow_x        = *(int32_t *)(&raw[18]);
    flow_y        = *(int32_t *)(&raw[22]);
    flow_distance = *(uint16_t *)(&raw[26]);
    flow_quality  = raw[28];

    /* 原子更新全局结构体（单次写入，主循环读取不会撕裂） */
    slave.baro_altitude = altitude;                     // 气压计高度 (cm)
    slave.mag_yaw       = yaw;                          // 磁力计航向 (0~360°)
    slave.flow_x        = flow_x;                       // 光流 X 原始值
    slave.flow_y        = flow_y;                       // 光流 Y 原始值
    slave.flow_distance = flow_distance;                // 光流测距 (mm)
    slave.flow_quality  = flow_quality;                 // 信号强度 (0~100)
    slave.flow_altitude = (float)flow_distance / 10.0f; // mm → cm

    slave.updated = 1;  /* 通知主循环有新数据 */
}
