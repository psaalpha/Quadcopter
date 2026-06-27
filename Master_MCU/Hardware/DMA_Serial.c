#include "stm32f10x.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

// 接收缓冲区 & 标志
char BlueSerial_RxPacket[100];
uint8_t BlueSerial_RxFlag;

// PID 全局参数
float PKp=0,PKi=0,PKd=0,Mid=0;
float RKp=0,RKi=0,RKd=0;
float YKp=0,YKi=0,YKd=0;
float PAKp=0,RAKp=0,YAKp=0;
float PAIM=0,RAIM=0;
float Contrl_Speed=0;

// DMA 发送缓冲区
#define USART_DMA_TX_BUF_LEN  64
uint8_t usart_dma_tx_buf[USART_DMA_TX_BUF_LEN];

void BlueSerial_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStruct;
    USART_InitTypeDef       USART_InitStruct;
    DMA_InitTypeDef         DMA_InitStruct;
    NVIC_InitTypeDef        NVIC_InitStruct;

    // 开启时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // PA9 TX 复用推挽
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA10 RX 上拉输入
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    // USART1 配置 38400 8N1
    USART_InitStruct.USART_BaudRate = 38400;
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStruct.USART_Parity = USART_Parity_No;
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART1, &USART_InitStruct);

    // DMA1_Channel4  USART1 TX 配置 【已修正宏名】
    DMA_DeInit(DMA1_Channel4);
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    DMA_InitStruct.DMA_MemoryBaseAddr     = (uint32_t)usart_dma_tx_buf;
    DMA_InitStruct.DMA_DIR                = 0;  // 修复点
    DMA_InitStruct.DMA_BufferSize         = USART_DMA_TX_BUF_LEN;
    DMA_InitStruct.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStruct.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStruct.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStruct.DMA_Priority           = DMA_Priority_Medium;
    DMA_Init(DMA1_Channel4, &DMA_InitStruct);

    // 开启串口接收中断
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    // 串口中断分组
    NVIC_InitStruct.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    // 使能 USART DMA 发送请求
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
    USART_Cmd(USART1, ENABLE);
}

// DMA 批量发送函数【修复版】
void BlueSerial_DMA_Send(uint8_t *buff, uint16_t len)
{
    if(len == 0 || len > USART_DMA_TX_BUF_LEN) 
    {
        return;
    }

    // 1. 等待上一次DMA传输完成
    while(DMA_GetFlagStatus(DMA1_FLAG_TC4) != RESET);
    
    // 2. 关闭DMA再配置长度（规范写法）
    DMA_Cmd(DMA1_Channel4, DISABLE);
    
    // 3. 拷贝数据 + 设置本次传输长度
    memcpy(usart_dma_tx_buf, buff, len);
    DMA_SetCurrDataCounter(DMA1_Channel4, len);
    
    // 4. 清除完成标志 + 开启DMA
    DMA_ClearFlag(DMA1_FLAG_TC4);
    DMA_Cmd(DMA1_Channel4, ENABLE);
}

// ========== 原有阻塞接口（保留用于调试） ==========
void BlueSerial_SendByte(uint8_t Byte)
{
    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, Byte);
}

void BlueSerial_SendArray(uint8_t *Array, uint16_t Length)
{
    uint16_t i;
    for(i = 0; i < Length; i++) BlueSerial_SendByte(Array[i]);
}

void BlueSerial_SendString(char *String)
{
    while(*String) BlueSerial_SendByte(*String++);
}

uint32_t BlueSerial_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while(Y--) Result *= X;
    return Result;
}

void BlueSerial_SendNumber(uint32_t Number, uint8_t Length)
{
    for(uint8_t i = 0; i < Length; i++)
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

void BlueSerial_SendBuff(uint8_t *Buff, uint16_t Len)
{
    uint16_t i;
    for(i = 0; i < Len; i++)
    {
        while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, Buff[i]);
    }
}

// 串口中断：仅处理接收
void USART1_IRQHandler(void)
{
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t RxData = USART_ReceiveData(USART1);
        static uint8_t RxState = 0;
        static uint8_t pRxPacket = 0;

        if(RxState == 0)
        {
            if(RxData == '[' && BlueSerial_RxFlag == 0)
            {
                RxState = 1;
                pRxPacket = 0;
            }
        }
        else if(RxState == 1)
        {
            if(RxData == ']')
            {
                RxState = 0;
                BlueSerial_RxPacket[pRxPacket] = '\0';
                BlueSerial_RxFlag = 1;
            }
            else
            {
                BlueSerial_RxPacket[pRxPacket] = RxData;
                pRxPacket ++;
            }
        }
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}

// ========== PID 解析 & 读参数函数（完全保留） ==========
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
            char *param_val = strtok(NULL, ",");
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

float Pitch_Back_Kp(void) { return PKp; }
float Pitch_Back_Ki(void) { return PKi; }
float Pitch_Back_Kd(void) { return PKd; }
float Back_Mid(void)      { return Mid; }
float Roll_Back_Kp(void)  { return RKp; }
float Roll_Back_Ki(void)  { return RKi; }
float Roll_Back_Kd(void)  { return RKd; }
float Yaw_Back_Kp(void)    { return YKp; }
float Yaw_Back_Ki(void)    { return YKi; }
float Yaw_Back_Kd(void)    { return YKd; }
float Roll_Back_Aim(void)  { return RAIM; }
float Pitch_Back_Aim(void) { return PAIM; }
uint8_t Back_Base_Duty(void){ return Contrl_Speed; }
float Pitch_Angle_Back_Kp(void){ return PAKp; }
float Roll_Angle_Back_Kp(void) { return RAKp; }
float Yaw_Angle_Back_Kp(void)  { return YAKp; }
