/**
  ******************************************************************************
  * @file    BlueSerial.c
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
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

/* ============================================
 * 全局变量（接口保持不变）
 * ============================================ */
char    BlueSerial_RxPacket[100];
uint8_t BlueSerial_RxFlag;

float PKp  = 0;   /* Pitch Kp */
float PKi  = 0;   /* Pitch Ki */
float PKd  = 0;   /* Pitch Kd */
float Mid  = 0;   /* 中点偏移 */
float RKp  = 0;   /* Roll Kp  */
float RKi  = 0;   /* Roll Ki  */
float RKd  = 0;   /* Roll Kd  */
float YKp  = 0;   /* Yaw Kp   */
float YKi  = 0;   /* Yaw Ki   */
float YKd  = 0;   /* Yaw Kd   */
float PAKp = 0;   /* Pitch 角度环 Kp */
float RAKp = 0;   /* Roll  角度环 Kp */
float YAKp = 0;   /* Yaw   角度环 Kp */
float PAIM = 0;   /* Pitch 目标角度 */
float RAIM = 0;   /* Roll  目标角度 */
float Contrl_Speed = 0;

/* ============================================
 * DMA 缓冲区
 * ============================================ */
#define BS_TX_BUF_SIZE      128u
#define BS_RX_DMA_BUF_SIZE  128u

static uint8_t bs_tx_buf[BS_TX_BUF_SIZE];
static uint8_t bs_rx_dma_buf[BS_RX_DMA_BUF_SIZE] __attribute__((aligned(4)));

static volatile uint8_t bs_tx_busy = 0;       /* DMA TX 忙标志（ISR 清零） */

/* RX 解析状态机 */
static uint8_t  bs_rx_state = 0;
static uint8_t  bs_rx_idx   = 0;
static uint32_t bs_rx_last_ndtr = BS_RX_DMA_BUF_SIZE;

/* ============================================
 * 静态函数声明
 * ============================================ */
static void BS_DMA_TX_Config(void);
static void BS_DMA_RX_Config(void);
static void BS_RxStateMachine(uint8_t byte);
static void BS_ExtractRxBytes(void);

/* ============================================
 * BlueSerial_Init
 * ============================================ */
void BlueSerial_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* ── 时钟 ── */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    /* ── PA9: USART1_TX (复用推挽) ── */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* ── PA10: USART1_RX (浮空输入 — DMA 接收推荐) ── */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* ── USART1: 38400-8N1 ── */
    USART_InitStructure.USART_BaudRate            = 38400;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_Init(USART1, &USART_InitStructure);

    /* ── DMA 配置 ── */
    BS_DMA_TX_Config();
    BS_DMA_RX_Config();

    /* ── 使能 USART DMA 请求 ── */
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);

    /* ── 使能 IDLE 中断 (替代 RXNE) ── */
    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);

    /* ── NVIC: USART1 (IDLE 中断) ── */
    NVIC_InitStructure.NVIC_IRQChannel                   = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 2;
    NVIC_Init(&NVIC_InitStructure);

    /* ── NVIC: DMA1_Channel4 (TX 完成中断) ── */
    NVIC_InitStructure.NVIC_IRQChannel                   = DMA1_Channel4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 3;
    NVIC_Init(&NVIC_InitStructure);

    /* ── 启动 USART1 ── */
    USART_Cmd(USART1, ENABLE);
}

/* ============================================
 * DMA1_Channel4 — USART1_TX
 * Normal 模式，每次发送时配置长度
 * ============================================ */
static void BS_DMA_TX_Config(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA1_Channel4);

    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(USART1->DR);
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)bs_tx_buf;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralDST;  /* 内存→外设 */
    DMA_InitStructure.DMA_BufferSize         = 0;                      /* 每次发送时设定 */
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel4, &DMA_InitStructure);

    /* 使能传输完成中断 */
    DMA_ITConfig(DMA1_Channel4, DMA_IT_TC, ENABLE);
}

/* ============================================
 * DMA1_Channel5 — USART1_RX
 * Circular 模式，持续接收
 * ============================================ */
static void BS_DMA_RX_Config(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA1_Channel5);

    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(USART1->DR);
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)bs_rx_dma_buf;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;  /* 外设→内存 */
    DMA_InitStructure.DMA_BufferSize         = BS_RX_DMA_BUF_SIZE;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel5, &DMA_InitStructure);

    /* 记录初始 NDTR */
    bs_rx_last_ndtr = BS_RX_DMA_BUF_SIZE;

    /* 立即启动 DMA 接收 */
    DMA_Cmd(DMA1_Channel5, ENABLE);
}

/* ============================================
 * DMA1_Channel4 中断 — TX 完成
 * ============================================ */
void DMA1_Channel4_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC4) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TC4);
        bs_tx_busy = 0;
    }
}

/* ============================================
 * USART1 中断 — 仅处理 IDLE（RX 帧结束）
 * ============================================ */
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_IDLE) != RESET)
    {
        /* 清除 IDLE 标志：先读 SR，再读 DR */
        volatile uint32_t tmp = USART1->SR;
        tmp = USART1->DR;
        (void)tmp;

        BS_ExtractRxBytes();
    }
}

/* ============================================
 * 从 DMA 环形缓冲区提取新字节，送入状态机
 * ============================================ */
static void BS_ExtractRxBytes(void)
{
    uint32_t ndtr     = DMA_GetCurrDataCounter(DMA1_Channel5);
    uint32_t received;

    /* 计算本轮新收到的字节数 */
    if (ndtr <= bs_rx_last_ndtr)
    {
        /* 正常情况：DMA 继续向下写 */
        received = bs_rx_last_ndtr - ndtr;
    }
    else
    {
        /* 环形回绕：DMA 回到缓冲区头部 */
        received = BS_RX_DMA_BUF_SIZE - ndtr + bs_rx_last_ndtr;
    }

    if (received > 0 && received < BS_RX_DMA_BUF_SIZE)
    {
        uint32_t start_ofs = (BS_RX_DMA_BUF_SIZE - bs_rx_last_ndtr) % BS_RX_DMA_BUF_SIZE;

        for (uint32_t i = 0; i < received; i++)
        {
            uint8_t byte = bs_rx_dma_buf[(start_ofs + i) % BS_RX_DMA_BUF_SIZE];
            BS_RxStateMachine(byte);
        }
    }

    bs_rx_last_ndtr = ndtr;
}

/* ============================================
 * RX 帧解析状态机
 * 帧格式: [tag,param,val]
 * 与旧版 RXNE 中断逻辑完全等价
 * ============================================ */
static void BS_RxStateMachine(uint8_t byte)
{
    if (bs_rx_state == 0)
    {
        if (byte == '[' && BlueSerial_RxFlag == 0)
        {
            bs_rx_state = 1;
            bs_rx_idx   = 0;
        }
    }
    else if (bs_rx_state == 1)
    {
        if (byte == ']')
        {
            bs_rx_state = 0;
            BlueSerial_RxPacket[bs_rx_idx] = '\0';
            BlueSerial_RxFlag = 1;
        }
        else
        {
            if (bs_rx_idx < 99)
            {
                BlueSerial_RxPacket[bs_rx_idx] = byte;
                bs_rx_idx++;
            }
        }
    }
}

/* ============================================
 * 等待 DMA TX 空闲
 * ============================================ */
static void BS_WaitTxIdle(void)
{
    while (bs_tx_busy)
    {
        /* 等待 DMA TC 中断清零 bs_tx_busy */
    }
}

/* ============================================
 * BlueSerial_SendBuff — DMA 批量发送（主循环使用）
 *
 * 非阻塞：启动 DMA 后立即返回
 * 若上次发送未完成则等待
 * ============================================ */
void BlueSerial_SendBuff(uint8_t *Buff, uint16_t Len)
{
    if (Len == 0 || Len > BS_TX_BUF_SIZE)
        return;

    /* 等待上次 DMA 传输完成 */
    BS_WaitTxIdle();

    /* 拷贝数据到 DMA 缓冲区（DMA 需要数据在传输期间保持有效） */
    memcpy(bs_tx_buf, Buff, Len);

    /* 重配 DMA 并启动 */
    DMA_Cmd(DMA1_Channel4, DISABLE);
    DMA_SetCurrDataCounter(DMA1_Channel4, Len);
    DMA_Cmd(DMA1_Channel4, ENABLE);

    bs_tx_busy = 1;
}

/* ============================================
 * BlueSerial_SendByte — 单字节阻塞发送（调试用）
 *
 * 等待 DMA TX 空闲 + 移位寄存器空，然后轮询发送
 * ============================================ */
void BlueSerial_SendByte(uint8_t Byte)
{
    BS_WaitTxIdle();

    /* 等待移位寄存器清空（上一帧最后一字节可能还在移位） */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);

    USART_SendData(USART1, Byte);

    /* 等待数据送入移位寄存器 */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
}

/* ============================================
 * 以下函数完全保持原有实现
 * ============================================ */

void BlueSerial_SendArray(uint8_t *Array, uint16_t Length)
{
    uint16_t i;
    for (i = 0; i < Length; i++)
    {
        BlueSerial_SendByte(Array[i]);
    }
}

void BlueSerial_SendString(char *String)
{
    uint8_t i;
    for (i = 0; String[i] != '\0'; i++)
    {
        BlueSerial_SendByte(String[i]);
    }
}

uint32_t BlueSerial_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while (Y--)
    {
        Result *= X;
    }
    return Result;
}

void BlueSerial_SendNumber(uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)
    {
        BlueSerial_SendByte(Number / BlueSerial_Pow(10, Length - i - 1) % 10 + '0');
    }
}

void BlueSerial_Printf(char *format, ...)
{
    char String[100];
    va_list arg;
    va_start(arg, format);
    vsprintf(String, format, arg);
    va_end(arg);
    BlueSerial_SendString(String);
}

/* ============================================
 * PID_Param_Parse — 完全保留原有逻辑
 * ============================================ */
void PID_Param_Parse(void)
{
    if (BlueSerial_RxFlag == 1)
    {
        char *tag = strtok(BlueSerial_RxPacket, ",");
        if (tag == NULL)
        {
            BlueSerial_RxFlag = 0;
            return;
        }

        if (strcmp(tag, "slider") == 0)
        {
            char *param_name = strtok(NULL, ",");
            char *param_val  = strtok(NULL, ",");

            if (param_name == NULL || param_val == NULL)
            {
                BlueSerial_RxFlag = 0;
                return;
            }

            float val = atof(param_val);

            if (strcmp(param_name, "PKp") == 0)
            {
                if (val >= 0.0f && val <= 40.0f) PKp = val;
            }
            else if (strcmp(param_name, "PKi") == 0)
            {
                if (val >= 0.0f && val <= 1.0f) PKi = val;
            }
            else if (strcmp(param_name, "PKd") == 0)
            {
                if (val >= 0.0f && val <= 5.0f) PKd = val;
            }
            else if (strcmp(param_name, "Mid") == 0)
            {
                if (val >= -10.0f && val <= 10.0f) Mid = val;
            }
            else if (strcmp(param_name, "RKp") == 0)
            {
                if (val >= 0.0f && val <= 10.0f) RKp = val;
            }
            else if (strcmp(param_name, "RKd") == 0)
            {
                if (val >= 0.0f && val <= 10.0f) RKd = val;
            }
            else if (strcmp(param_name, "RKi") == 0)
            {
                if (val >= 0.0f && val <= 10.0f) RKi = val;
            }
            else if (strcmp(param_name, "YKp") == 0)
            {
                if (val >= 0.0f && val <= 10.0f) YKp = val;
            }
            else if (strcmp(param_name, "YKd") == 0)
            {
                if (val >= 0.0f && val <= 10.0f) YKd = val;
            }
            else if (strcmp(param_name, "YKi") == 0)
            {
                if (val >= 0.0f && val <= 10.0f) YKi = val;
            }
            else if (strcmp(param_name, "PAKp") == 0)
            {
                if (val >= 0.0f && val <= 10.0f) PAKp = val;
            }
            else if (strcmp(param_name, "RAKp") == 0)
            {
                if (val >= 0.0f && val <= 10.0f) RAKp = val;
            }
            else if (strcmp(param_name, "YAKp") == 0)
            {
                if (val >= 0.0f && val <= 10.0f) YAKp = val;
            }
            else if (strcmp(param_name, "PAIM") == 0)
            {
                if (val >= -100.0f && val <= 100.0f) PAIM = val;
            }
            else if (strcmp(param_name, "RAIM") == 0)
            {
                if (val >= -100.0f && val <= 100.0f) RAIM = val;
            }
            else if (strcmp(param_name, "Contrl_Speed") == 0)
            {
                if (val >= 0 && val <= 100) Contrl_Speed = val;
            }
        }

        BlueSerial_RxFlag = 0;
        memset(BlueSerial_RxPacket, 0, 100);
    }
}

/* ============================================
 * PID 参数读接口（完全保留）
 * ============================================ */
float Pitch_Back_Kp(void)        { return PKp; }
float Pitch_Back_Ki(void)        { return PKi; }
float Pitch_Back_Kd(void)        { return PKd; }
float Back_Mid(void)             { return Mid; }
float Roll_Back_Kp(void)         { return RKp; }
float Roll_Back_Ki(void)         { return RKi; }
float Roll_Back_Kd(void)         { return RKd; }
float Yaw_Back_Kp(void)          { return YKp; }
float Yaw_Back_Ki(void)          { return YKi; }
float Yaw_Back_Kd(void)          { return YKd; }
float Roll_Back_Aim(void)        { return RAIM; }
float Pitch_Back_Aim(void)       { return PAIM; }
uint8_t Back_Base_Duty(void)     { return Contrl_Speed; }
float Pitch_Angle_Back_Kp(void)  { return PAKp; }
float Roll_Angle_Back_Kp(void)   { return RAKp; }
float Yaw_Angle_Back_Kp(void)    { return YAKp; }
