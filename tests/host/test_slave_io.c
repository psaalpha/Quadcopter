#include "stm32f10x.h"
#include "optical_flow.h"
#include "master_link.h"
#include "board_inputs.h"
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
    optical_flow_data_t snapshot;
    optical_flow_stats_t stats;
    unsigned i;
    optical_flow_init();
    Feed(flow, 7u); Idle(); optical_flow_process(1u);
    assert(optical_flow_is_valid() == 0u); /* A partial header survives IDLE. */
    Feed(flow + 7u, 11u); Feed(range, 14u); Idle();
    /* ISR must not parse or update measurements. */
    assert(optical_flow_is_valid() == 0u);
    optical_flow_process(2u);
    optical_flow_get_snapshot(&snapshot);
    assert(snapshot.flow_x == 36 && snapshot.flow_y == -1);
    assert(snapshot.distance == 1500u && snapshot.signal_strength == 80u);
    assert(snapshot.checksum_valid == 0u); /* Original unconfirmed protocol. */
    /* Continuous stream with no IDLE: HT notification, bounded batches. */
    for (i = 0u; i < 8u; ++i) Feed(flow, sizeof(flow));
    for (i = 0u; i < 4u; ++i) optical_flow_process(3u);
    optical_flow_get_stats(&stats);
    assert(stats.flow_frames >= 6u);
    Idle();
    for (i = 0u; i < 4u; ++i) optical_flow_process(4u);
    optical_flow_get_stats(&stats);
    assert(stats.flow_frames == 9u);
    Feed(noise, sizeof(noise));
    optical_flow_process(5u);
    optical_flow_get_stats(&stats);
    assert(stats.buffer_overruns == 1u && optical_flow_is_valid() == 0u);
    Feed(flow, sizeof(flow)); Idle(); optical_flow_process(UINT32_MAX - 50u);
    assert(optical_flow_is_valid() == 1u);
    optical_flow_process(150u); /* 201ms elapsed across wrap. */
    assert(optical_flow_is_valid() == 0u);
    Feed(flow, 10u); Idle(); optical_flow_process(200u);
    optical_flow_pause();
    assert(DMA1_Channel5->enabled == 0u);
    optical_flow_resume();
    Feed(flow, sizeof(flow)); Idle(); optical_flow_process(201u);
    assert(optical_flow_is_valid() == 1u); /* Half-frame was discarded before flash. */
    fake_dma_flags |= DMA1_FLAG_TE5;
    DMA1_Channel5_IRQHandler();
    optical_flow_process(202u);
    optical_flow_get_stats(&stats);
    assert(stats.hardware_errors == 1u && stats.flash_pauses == 1u);
    assert(optical_flow_is_valid() == 0u && DMA1_Channel5->enabled != 0u);
}
static void TestTransmit(void)
{
    inter_mcu_sensor_data_t packet;
    master_link_stats_t stats;
    uint8_t saved[INTER_MCU_FRAME_SIZE];
    memset(&packet, 0, sizeof(packet));
    packet.sequence = 1u;
    master_link_init();
    assert(master_link_send(&packet, 0u) == 1u);
    memcpy(saved, DMA1_Channel7->memory, sizeof(saved));
    packet.sequence = 2u;
    assert(master_link_send(&packet, 1u) == 0u);
    assert(memcmp(saved, DMA1_Channel7->memory, sizeof(saved)) == 0);
    fake_dma_flags |= DMA1_FLAG_TC7;
    DMA1_Channel7_IRQHandler();
    assert(master_link_is_busy() != 0u); /* Completion consumed by main only. */
    master_link_process(4u);
    assert(master_link_is_busy() == 0u);
    assert(master_link_send(&packet, 50u) == 1u);
    fake_dma_flags |= DMA1_FLAG_TE7;
    DMA1_Channel7_IRQHandler();
    master_link_process(51u);
    assert(master_link_is_busy() == 0u);
    assert(master_link_send(&packet, UINT32_MAX - 9u) == 1u);
    master_link_process(10u);
    master_link_get_stats(&stats);
    assert(stats.sent == 1u && stats.skipped_busy == 1u && stats.dma_errors == 2u);
    assert(master_link_is_busy() == 0u);
}
static void TestInputs(void)
{
    board_input_events_t events;
    board_inputs_init();
    board_inputs_take(&events);
    assert(events.servo_changed != 0u);
    GPIOA->input = GPIO_Pin_8;
    fake_exti_pending = EXTI_Line8;
    EXTI9_5_IRQHandler(); /* Rising without a captured falling edge. */
    board_inputs_take(&events);
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
    board_inputs_take(&events);
    assert(events.key != 0u && events.servo_changed != 0u && events.calibration_pulse != 0u);
    assert(fake_interrupt_mask == 1u);
    board_inputs_take(&events);
    assert(events.key == 0u && events.servo_changed == 0u && events.calibration_pulse == 0u);
    fake_interrupt_mask = 0u;
    fake_exti_pending = EXTI_Line1;
    EXTI1_IRQHandler();
    board_inputs_take(&events);
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
