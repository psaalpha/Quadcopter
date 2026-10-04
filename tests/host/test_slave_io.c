#include "stm32f10x.h"
#include "OpticalFlow.h"
#include "slave_link.h"
#include "exti.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern uint32_t fake_dma_flags, fake_interrupt_mask, fake_exti_pending;
void USART1_IRQHandler(void);
void DMA1_Channel5_IRQHandler(void);
void DMA1_Channel7_IRQHandler(void);
void EXTI0_IRQHandler(void);
void EXTI1_IRQHandler(void);
void EXTI9_5_IRQHandler(void);
static void Feed(const uint8_t *bytes, unsigned count)
{
    unsigned i;
    for (i = 0u; i < count; ++i) {
        assert(DMA1_Channel5->enabled != 0u);
        DMA1_Channel5->memory[DMA1_Channel5->size - DMA1_Channel5->remaining] = bytes[i];
        DMA1_Channel5->remaining--;
        if (DMA1_Channel5->remaining == DMA1_Channel5->size / 2u) {
            fake_dma_flags |= DMA1_FLAG_HT5;
            DMA1_Channel5_IRQHandler();
        }
        if (DMA1_Channel5->remaining == 0u) {
            DMA1_Channel5->remaining = DMA1_Channel5->size;
            fake_dma_flags |= DMA1_FLAG_TC5;
            DMA1_Channel5_IRQHandler();
        }
    }
}
static void Idle(void)
{
    USART1->SR = USART_FLAG_IDLE;
    USART1_IRQHandler();
    USART1->SR = 0u;
}
static void TestOpticalFlow(void)
{
    uint8_t flow[18] = {0x24u,0x58u,0x3Cu,0u,2u,0x1Fu,9u,0u,0xF5u,
                       0x24u,0u,0u,0u,0xFFu,0xFFu,0xFFu,0xFFu,0u};
    uint8_t range[14] = {0x24u,0x58u,0x3Cu,0u,1u,0x1Fu,5u,0u,80u,
                         0xDCu,5u,1u,0u,0u};
    uint8_t noise[256] = {0u};
    OpticalFlow_Data_t snapshot;
    OpticalFlow_Stats stats;
    unsigned i;
    OpticalFlow_Init();
    Feed(flow, 7u); Idle(); OpticalFlow_Process(1u);
    assert(IsDataValid() == 0u); /* A partial header survives IDLE. */
    Feed(flow + 7u, 11u); Feed(range, 14u); Idle();
    /* ISR must not parse or update measurements. */
    assert(IsDataValid() == 0u);
    OpticalFlow_Process(2u);
    OpticalFlow_GetSnapshot(&snapshot);
    assert(snapshot.flow_x == 36 && snapshot.flow_y == -1);
    assert(snapshot.distance == 1500u && snapshot.signal_strength == 80u);
    assert(snapshot.checksum_valid == 0u); /* Original unconfirmed protocol. */
    /* Continuous stream with no IDLE: HT notification, bounded batches. */
    for (i = 0u; i < 8u; ++i) Feed(flow, sizeof(flow));
    for (i = 0u; i < 4u; ++i) OpticalFlow_Process(3u);
    OpticalFlow_GetStats(&stats);
    assert(stats.flow_frames >= 6u);
    Idle();
    for (i = 0u; i < 4u; ++i) OpticalFlow_Process(4u);
    OpticalFlow_GetStats(&stats);
    assert(stats.flow_frames == 9u);
    Feed(noise, sizeof(noise));
    OpticalFlow_Process(5u);
    OpticalFlow_GetStats(&stats);
    assert(stats.buffer_overruns == 1u && IsDataValid() == 0u);
    Feed(flow, sizeof(flow)); Idle(); OpticalFlow_Process(UINT32_MAX - 50u);
    assert(IsDataValid() == 1u);
    OpticalFlow_Process(150u); /* 201ms elapsed across wrap. */
    assert(IsDataValid() == 0u);
    Feed(flow, 10u); Idle(); OpticalFlow_Process(200u);
    OpticalFlow_Pause();
    assert(DMA1_Channel5->enabled == 0u);
    OpticalFlow_Resume();
    Feed(flow, sizeof(flow)); Idle(); OpticalFlow_Process(201u);
    assert(IsDataValid() == 1u); /* Half-frame was discarded before flash. */
    fake_dma_flags |= DMA1_FLAG_TE5;
    DMA1_Channel5_IRQHandler();
    OpticalFlow_Process(202u);
    OpticalFlow_GetStats(&stats);
    assert(stats.hardware_errors == 1u && stats.flash_pauses == 1u);
    assert(IsDataValid() == 0u && DMA1_Channel5->enabled != 0u);
}
static void TestTransmit(void)
{
    InterMcuSensorData packet;
    SlaveLinkStats stats;
    uint8_t saved[INTER_MCU_FRAME_SIZE];
    memset(&packet, 0, sizeof(packet));
    packet.sequence = 1u;
    SlaveLink_Init();
    assert(SlaveLink_Send(&packet, 0u) == 1u);
    memcpy(saved, DMA1_Channel7->memory, sizeof(saved));
    packet.sequence = 2u;
    assert(SlaveLink_Send(&packet, 1u) == 0u);
    assert(memcmp(saved, DMA1_Channel7->memory, sizeof(saved)) == 0);
    fake_dma_flags |= DMA1_FLAG_TC7;
    DMA1_Channel7_IRQHandler();
    assert(SlaveLink_IsBusy() != 0u); /* Completion consumed by main only. */
    SlaveLink_Process(4u);
    assert(SlaveLink_IsBusy() == 0u);
    assert(SlaveLink_Send(&packet, 50u) == 1u);
    fake_dma_flags |= DMA1_FLAG_TE7;
    DMA1_Channel7_IRQHandler();
    SlaveLink_Process(51u);
    assert(SlaveLink_IsBusy() == 0u);
    assert(SlaveLink_Send(&packet, UINT32_MAX - 9u) == 1u);
    SlaveLink_Process(10u);
    SlaveLink_GetStats(&stats);
    assert(stats.sent == 1u && stats.skipped_busy == 1u && stats.dma_errors == 2u);
    assert(SlaveLink_IsBusy() == 0u);
}
static void TestInputs(void)
{
    SlaveInputEvents events;
    EXTI_Inputs_Init();
    EXTI_Inputs_Take(&events);
    assert(events.servo_changed != 0u);
    GPIOA->input = GPIO_Pin_8;
    fake_exti_pending = EXTI_Line8;
    EXTI9_5_IRQHandler(); /* Rising without a captured falling edge. */
    EXTI_Inputs_Take(&events);
    assert(events.calibration_pulse == 0u);
    GPIOA->input = 0u;
    fake_exti_pending = EXTI_Line8;
    EXTI9_5_IRQHandler();
    GPIOA->input = GPIO_Pin_8;
    fake_exti_pending = EXTI_Line8;
    EXTI9_5_IRQHandler(); /* Entire pulse before main loop takes events. */
    fake_exti_pending = EXTI_Line0 | EXTI_Line1;
    EXTI0_IRQHandler(); EXTI1_IRQHandler();
    fake_interrupt_mask = 1u;
    EXTI_Inputs_Take(&events);
    assert(events.key != 0u && events.servo_changed != 0u && events.calibration_pulse != 0u);
    assert(fake_interrupt_mask == 1u);
    EXTI_Inputs_Take(&events);
    assert(events.key == 0u && events.servo_changed == 0u && events.calibration_pulse == 0u);
    fake_interrupt_mask = 0u;
    fake_exti_pending = EXTI_Line1;
    EXTI1_IRQHandler();
    EXTI_Inputs_Take(&events);
    assert(events.servo_changed != 0u && fake_interrupt_mask == 0u);
}
int main(void)
{
    TestOpticalFlow();
    TestTransmit();
    TestInputs();
    puts("slave IO tests passed");
    return 0;
}
