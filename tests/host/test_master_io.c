#include "stm32f10x.h"
#include "dma_rx.h"
#include "BlueSerial.h"
#include "SlaveMCU.h"
#include "PWM4.h"
#include "inter_mcu_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern uint32_t fake_dma_flags, fake_interrupt_mask;
void USART1_IRQHandler(void);
void USART3_IRQHandler(void);
void DMA1_Channel3_IRQHandler(void);
void DMA1_Channel4_IRQHandler(void);
void DMA1_Channel5_IRQHandler(void);

static void feed(DMA_Channel_TypeDef *dma, const uint8_t *bytes, unsigned length,
                  uint32_t half_flag, uint32_t full_flag, void (*irq)(void))
{
    unsigned i;
    for(i=0;i<length;++i) {
        assert(dma->enabled);
        dma->memory[dma->size-dma->remaining]=bytes[i];
        --dma->remaining;
        if(dma->remaining==dma->size/2u) {
            fake_dma_flags |= half_flag;
            if(irq) irq();
        }
        if(dma->remaining==0u) {
            dma->remaining=dma->size;
            fake_dma_flags |= full_flag;
            if(irq) irq();
        }
    }
}
static void slave_feed(const uint8_t *bytes,unsigned length) {
    feed(DMA1_Channel3,bytes,length,DMA1_FLAG_HT3,DMA1_FLAG_TC3,DMA1_Channel3_IRQHandler);
    USART3->SR=1u; USART3_IRQHandler(); USART3->SR=0u;
}
static void blue_feed(const char *text) {
    feed(DMA1_Channel5,(const uint8_t *)text,(unsigned)strlen(text),DMA1_FLAG_HT5,DMA1_FLAG_TC5,DMA1_Channel5_IRQHandler);
    USART1->SR=1u; USART1_IRQHandler(); USART1->SR=0u;
}
static void slave_service(void) {
    unsigned i;
    for(i=0;i<8u;++i) SlaveMCU_Process();
}
static void make_frame(uint8_t *frame,uint16_t sequence) {
    InterMcuSensorData data;
    memset(&data,0,sizeof(data));
    data.sequence=sequence; data.timestamp_ms=50u*sequence;
    data.baro_altitude_mm=-1230; data.yaw_centi_deg=12345;
    data.flow_distance_mm=750; data.flow_x=-44; data.battery_mv=12000;
    assert(InterMcu_EncodeSensorFrame(&data,frame,INTER_MCU_FRAME_SIZE));
}
static void test_slave(void) {
    uint8_t frame[INTER_MCU_FRAME_SIZE];
    uint8_t noise[220];
    memset(noise,0,sizeof(noise));
    SlaveMCU_Init(); make_frame(frame,1u);
    slave_feed(frame,10u);
    assert(slave.frames_received==0u); /* ISR never decodes. */
    slave_service(); assert(slave.frames_received==0u);
    slave_feed(frame+10u,INTER_MCU_FRAME_SIZE-10u);
    assert(slave.frames_received==0u);
    slave_service(); assert(slave.frames_received==1u);
    assert(slave.baro_altitude==-123.0f && slave.mag_yaw==123.45f);
    assert(slave.flow_altitude==75.0f && slave.flow_x==-44);
    make_frame(frame,2u); slave_feed(frame,sizeof(frame));
    make_frame(frame,3u); slave_feed(frame,sizeof(frame));
    slave_service(); assert(slave.frames_received==3u && slave.sequence==3u);
    assert(slave.sequence_gaps==0u);
    frame[10]^=1u; slave_feed(frame,sizeof(frame));
    make_frame(frame,4u); slave_feed(frame,sizeof(frame));
    slave_service(); assert(slave.sequence==4u && slave.crc_errors==1u);
    /* Force a frame across the circular buffer boundary. */
    slave_feed(noise,sizeof(noise)); slave_service();
    make_frame(frame,5u); slave_feed(frame,sizeof(frame));
    slave_service(); assert(slave.sequence==5u);
    /* Lose a partial frame to overrun, then accept a clean subsequent frame. */
    slave_feed(frame,12u); slave_service();
    slave_feed(noise,sizeof(noise)); slave_feed(noise,80u);
    slave_service(); assert(slave.rx_overruns==1u);
    make_frame(frame,6u); slave_feed(frame,sizeof(frame));
    slave_service(); assert(slave.sequence==6u);
}
static void test_blue(void) {
    char overlong[140];
    uint32_t mask;
    unsigned i;
    BlueSerial_Init();
    blue_feed("[slider,PKp,1][slider,PKp,2][slider,PKi,0.5]");
    assert(BlueSerial_RxFlag==0u && Pitch_Back_Kp()==0.0f);
    assert(BlueSerial_Process()); mask=PID_Param_Parse();
    assert(mask==PID_PARAM_UPDATE_PKP && Pitch_Back_Kp()==1.0f);
    assert(BlueSerial_Process()); PID_Param_Parse(); assert(Pitch_Back_Kp()==2.0f);
    assert(BlueSerial_Process()); PID_Param_Parse(); assert(Pitch_Back_Ki()==0.5f);
    assert(!BlueSerial_Process());
    blue_feed("[slider,PKp,"); assert(!BlueSerial_Process());
    blue_feed("3]"); assert(BlueSerial_Process()); PID_Param_Parse(); assert(Pitch_Back_Kp()==3.0f);
    memset(overlong,'a',sizeof(overlong)); overlong[0]='[';
    overlong[sizeof(overlong)-2u]=']'; overlong[sizeof(overlong)-1u]='\0';
    blue_feed(overlong); for(i=0;i<3u;++i) assert(!BlueSerial_Process());
    assert(BlueSerial_GetRxFrameErrors()==1u);
    blue_feed("[slider,PKp,4]"); assert(BlueSerial_Process()); PID_Param_Parse();
    assert(Pitch_Back_Kp()==4.0f);
    /* Continuous input: half-full IRQ can wake service without IDLE. */
    memset(overlong,'x',128u);
    feed(DMA1_Channel5,(uint8_t *)overlong,128u,DMA1_FLAG_HT5,DMA1_FLAG_TC5,DMA1_Channel5_IRQHandler);
    for(i=0;i<3u;++i) assert(!BlueSerial_Process());
    blue_feed("[slider,PKp,5]"); assert(BlueSerial_Process()); PID_Param_Parse();
    assert(Pitch_Back_Kp()==5.0f);
    for(i=0;i<3u;++i) feed(DMA1_Channel5,(uint8_t *)overlong,128u,DMA1_FLAG_HT5,DMA1_FLAG_TC5,DMA1_Channel5_IRQHandler);
    assert(!BlueSerial_Process()); assert(BlueSerial_GetRxOverruns()==1u);
    blue_feed("[slider,PKp,6]"); assert(BlueSerial_Process()); PID_Param_Parse();
    assert(Pitch_Back_Kp()==6.0f);
    /* Busy DMA telemetry is skipped, not waited on or overwritten. */
    BlueSerial_SendBuff((uint8_t *)"one",3u);
    BlueSerial_SendBuff((uint8_t *)"two",3u);
    assert(memcmp(DMA1_Channel4->memory,"one",3u)==0);
    fake_dma_flags |= DMA1_IT_TC4; DMA1_Channel4_IRQHandler();
    BlueSerial_SendBuff((uint8_t *)"two",3u);
    assert(memcmp(DMA1_Channel4->memory,"two",3u)==0);
}
static void test_cursor(void) {
    static uint8_t ring[256];
    uint8_t input[256],output[64],dropped;
    DmaRx rx;
    unsigned i;
    DMA_Channel_TypeDef channel;
    memset(&channel,0,sizeof(channel)); channel.remaining=256u; channel.size=256u;
    channel.memory=ring; channel.enabled=1u;
    memset(input,0x55,sizeof(input));
    fake_dma_flags=0u;
    DmaRx_Init(&rx,&channel,ring,256u,DMA1_FLAG_HT3,DMA1_FLAG_TC3);
    feed(&channel,input,100u,DMA1_FLAG_HT3,DMA1_FLAG_TC3,NULL);
    DmaRx_NotifyFromIsr(&rx);
    fake_interrupt_mask=1u;
    assert(DmaRx_Read(&rx,output,sizeof(output),&dropped)==64u && !dropped);
    assert(fake_interrupt_mask==1u); fake_interrupt_mask=0u;
    assert(DmaRx_Read(&rx,output,sizeof(output),&dropped)==36u);
    /* Pending TC is handled in snapshot even before the IRQ runs. */
    feed(&channel,input,156u,DMA1_FLAG_HT3,DMA1_FLAG_TC3,NULL);
    DmaRx_NotifyFromIsr(&rx);
    assert(DmaRx_Read(&rx,output,sizeof(output),&dropped)==64u && !dropped);
    assert(rx.wraps==1u);
    DmaRx_OnDmaInterrupt(&rx); assert(rx.wraps==1u);
    for(i=0;i<3u;++i) (void)DmaRx_Read(&rx,output,sizeof(output),&dropped);
    /* uint32_t byte counter rollover. */
    rx.wraps=0x00ffffffu; rx.consumed=0xfffffff0u; channel.remaining=16u;
    feed(&channel,input,32u,DMA1_FLAG_HT3,DMA1_FLAG_TC3,NULL);
    DmaRx_NotifyFromIsr(&rx);
    assert(DmaRx_Read(&rx,output,sizeof(output),&dropped)==32u && !dropped);
    assert(rx.consumed==16u);
}
static void pwm_update(void) {
    unsigned i;
    if(fake_tim4.update_disabled) return;
    for(i=0;i<4u;++i) if(fake_tim4.preload[i]) fake_tim4.active[i]=fake_tim4.pending[i];
}
static void test_pwm(void) {
    unsigned i;
    PWM4_Init();
    PWM4_SetCompare1(900u); PWM4_SetCompare2(800u);
    assert(fake_tim4.active[0]==500u && fake_tim4.active[1]==500u);
    pwm_update(); assert(fake_tim4.active[0]==900u && fake_tim4.active[1]==800u);
    PWM4_SetCompare1(1000u); PWM4_SetMinimumOutput();
    for(i=0;i<4u;++i) assert(fake_tim4.active[i]==500u && fake_tim4.preload[i]);
    pwm_update(); for(i=0;i<4u;++i) assert(fake_tim4.active[i]==500u);
}
int main(void) {
    test_slave(); test_blue(); test_cursor(); test_pwm();
    puts("master_io_test: PASS"); return 0;
}
