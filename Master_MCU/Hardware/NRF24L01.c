#include "stm32f10x.h"
#include "NRF24L01_Define.h"
#include "Delay.h"

/*===== 全局变量 =====*/
uint8_t NRF24L01_TxAddress[5] = {0x11,0x22,0x33,0x44,0x55};
#define NRF24L01_TX_PACKET_WIDTH 4
uint8_t NRF24L01_TxPacket[NRF24L01_TX_PACKET_WIDTH];

uint8_t NRF24L01_RxAddress[5] = {0x11,0x22,0x33,0x44,0x55};
#define NRF24L01_RX_PACKET_WIDTH 4
uint8_t NRF24L01_RxPacket[NRF24L01_RX_PACKET_WIDTH];

// 🔴【新增】中断标志（必须 volatile）
volatile uint8_t NRF24L01_RxIrqFlag = 0;

/*===== 引脚底层 =====*/
void NRF24L01_W_CE(uint8_t BitValue)
{
    GPIO_WriteBit(GPIOA, GPIO_Pin_0, (BitAction)BitValue);
}
void NRF24L01_W_CSN(uint8_t BitValue)
{
    GPIO_WriteBit(GPIOA, GPIO_Pin_1, (BitAction)BitValue);
}
void NRF24L01_W_SCK(uint8_t BitValue)
{
    GPIO_WriteBit(GPIOA, GPIO_Pin_5, (BitAction)BitValue);
}
void NRF24L01_W_MOSI(uint8_t BitValue)
{
    GPIO_WriteBit(GPIOA, GPIO_Pin_6, (BitAction)BitValue);
}
uint8_t NRF24L01_R_MISO(void)
{
    return GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4);
}

/*===== GPIO初始化 =====*/
void NRF24L01_GPIO_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_5|GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    NRF24L01_W_CE(0);
    NRF24L01_W_CSN(1);
    NRF24L01_W_SCK(0);
    NRF24L01_W_MOSI(0);
}

// 🔴PB5 IRQ 外部中断初始化
void NRF24L01_IRQ_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    EXTI_InitTypeDef EXTI_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource5);

    EXTI_InitStruct.EXTI_Line = EXTI_Line5;
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStruct);

    NVIC_InitStruct.NVIC_IRQChannel = EXTI9_5_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);
}

/*===== SPI、寄存器读写 =====*/
uint8_t NRF24L01_SPI_SwapByte(uint8_t Byte)
{
    uint8_t i;
    for(i=0;i<8;i++){
        NRF24L01_W_MOSI(Byte&0x80 ? 1:0);
        Byte <<=1;
        NRF24L01_W_SCK(1);
        if(NRF24L01_R_MISO()) Byte |=1;
        NRF24L01_W_SCK(0);
    }
    return Byte;
}
uint8_t NRF24L01_ReadReg(uint8_t Reg){
    uint8_t d;
    NRF24L01_W_CSN(0);
    NRF24L01_SPI_SwapByte(NRF24L01_R_REGISTER|Reg);
    d=NRF24L01_SPI_SwapByte(NRF24L01_NOP);
    NRF24L01_W_CSN(1);
    return d;
}
void NRF24L01_WriteReg(uint8_t Reg,uint8_t d){
    NRF24L01_W_CSN(0);
    NRF24L01_SPI_SwapByte(NRF24L01_W_REGISTER|Reg);
    NRF24L01_SPI_SwapByte(d);
    NRF24L01_W_CSN(1);
}
void NRF24L01_ReadRegs(uint8_t Reg,uint8_t *buf,uint8_t n){
    NRF24L01_W_CSN(0);
    NRF24L01_SPI_SwapByte(NRF24L01_R_REGISTER|Reg);
    for(uint8_t i=0;i<n;i++) buf[i]=NRF24L01_SPI_SwapByte(NRF24L01_NOP);
    NRF24L01_W_CSN(1);
}
void NRF24L01_WriteRegs(uint8_t Reg,uint8_t *buf,uint8_t n){
    NRF24L01_W_CSN(0);
    NRF24L01_SPI_SwapByte(NRF24L01_W_REGISTER|Reg);
    for(uint8_t i=0;i<n;i++) NRF24L01_SPI_SwapByte(buf[i]);
    NRF24L01_W_CSN(1);
}
void NRF24L01_ReadRxPayload(uint8_t *buf,uint8_t n){
    NRF24L01_W_CSN(0);
    NRF24L01_SPI_SwapByte(NRF24L01_R_RX_PAYLOAD);
    for(uint8_t i=0;i<n;i++) buf[i]=NRF24L01_SPI_SwapByte(NRF24L01_NOP);
    NRF24L01_W_CSN(1);
}
void NRF24L01_WriteTxPayload(uint8_t *buf,uint8_t n){
    NRF24L01_W_CSN(0);
    NRF24L01_SPI_SwapByte(NRF24L01_W_TX_PAYLOAD);
    for(uint8_t i=0;i<n;i++) NRF24L01_SPI_SwapByte(buf[i]);
    NRF24L01_W_CSN(1);
}
void NRF24L01_FlushTx(void){ NRF24L01_W_CSN(0); NRF24L01_SPI_SwapByte(NRF24L01_FLUSH_TX); NRF24L01_W_CSN(1); }
void NRF24L01_FlushRx(void){ NRF24L01_W_CSN(0); NRF24L01_SPI_SwapByte(NRF24L01_FLUSH_RX); NRF24L01_W_CSN(1); }
uint8_t NRF24L01_ReadStatus(void){
    uint8_t s;
    NRF24L01_W_CSN(0);
    s=NRF24L01_SPI_SwapByte(NRF24L01_NOP);
    NRF24L01_W_CSN(1);
    return s;
}

/*===== 模式函数 =====*/
void NRF24L01_Rx(void)
{
    uint8_t c=NRF24L01_ReadReg(NRF24L01_CONFIG);
    NRF24L01_W_CE(0);
    c|=0x03;
    NRF24L01_WriteReg(NRF24L01_CONFIG,c);
    NRF24L01_W_CE(1);
}
void NRF24L01_Tx(void){
    uint8_t c=NRF24L01_ReadReg(NRF24L01_CONFIG);
    NRF24L01_W_CE(0);
    c=(c|0x02)&~0x01;
    NRF24L01_WriteReg(NRF24L01_CONFIG,c);
    NRF24L01_W_CE(1);
}

// 🔴初始化里 CONFIG 寄存器值
void NRF24L01_Init(void)
{
    NRF24L01_GPIO_Init();

    // 🔴原来 0x0A → 现在 0x0F（开启接收中断）
    NRF24L01_WriteReg(NRF24L01_CONFIG, 0x0F);

    NRF24L01_WriteReg(NRF24L01_EN_AA, 0x01);
    NRF24L01_WriteReg(NRF24L01_EN_RXADDR, 0x01);
    NRF24L01_WriteReg(NRF24L01_SETUP_AW, 0x03);
    NRF24L01_WriteReg(NRF24L01_SETUP_RETR, 0x1F);
    NRF24L01_WriteReg(NRF24L01_RF_CH, 0x02);
    NRF24L01_WriteReg(NRF24L01_RF_SETUP, 0x06);
    NRF24L01_WriteReg(NRF24L01_RX_PW_P0, NRF24L01_RX_PACKET_WIDTH);
    NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0, NRF24L01_RxAddress,5);

    NRF24L01_FlushTx();
    NRF24L01_FlushRx();
    NRF24L01_WriteReg(NRF24L01_STATUS,0x70);

    NRF24L01_Rx();
}

/*===== 发送、接收 =====*/
uint8_t NRF24L01_Send(void)
{
    uint8_t Status,SendFlag=0;
    uint32_t Timeout=10000;
    NRF24L01_WriteRegs(NRF24L01_TX_ADDR,NRF24L01_TxAddress,5);
    NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0,NRF24L01_TxAddress,5);
    NRF24L01_WriteTxPayload(NRF24L01_TxPacket,NRF24L01_TX_PACKET_WIDTH);
    NRF24L01_Tx();
    while(1){
        Status=NRF24L01_ReadStatus();
        if(Timeout--==0){SendFlag=4;NRF24L01_Init();break;}
        if((Status&0x30)==0x30){SendFlag=3;NRF24L01_Init();break;}
        else if(Status&0x10){SendFlag=2;NRF24L01_Init();break;}
        else if(Status&0x20){SendFlag=1;break;}
    }
    NRF24L01_WriteReg(NRF24L01_STATUS,0x30);
    NRF24L01_FlushTx();
    NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0,NRF24L01_RxAddress,5);
    NRF24L01_Rx();
    return SendFlag;
}

uint8_t NRF24L01_Receive(void)
{
    uint8_t Status,Config,ReceiveFlag=0;
    Status=NRF24L01_ReadStatus();
    Config=NRF24L01_ReadReg(NRF24L01_CONFIG);
    if((Config&0x02)==0){ReceiveFlag=3;NRF24L01_Init();}
    else if((Status&0x30)==0x30){ReceiveFlag=2;NRF24L01_Init();}
    else if(Status&0x40){
        ReceiveFlag=1;
        NRF24L01_ReadRxPayload(NRF24L01_RxPacket,NRF24L01_RX_PACKET_WIDTH);
        NRF24L01_WriteReg(NRF24L01_STATUS,0x40);
        NRF24L01_FlushRx();
    }
    return ReceiveFlag;
}

void NRF24L01_UpdateRxAddress(void)
{
    NRF24L01_WriteRegs(NRF24L01_RX_ADDR_P0,NRF24L01_RxAddress,5);
}
