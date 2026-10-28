#include "crsf.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern uint32_t fake_dma_flags;
void DMA1_Channel6_IRQHandler(void);
void USART2_IRQHandler(void);
static uint8_t crc8(const uint8_t *bytes, unsigned count)
{
    uint8_t crc = 0u;
    unsigned i;
    while (count-- != 0u) {
        crc ^= *bytes++;
        for (i = 0u; i < 8u; ++i) crc = (uint8_t)((crc & 0x80u) ? ((uint32_t)crc << 1) ^ 0xd5u : (uint32_t)crc << 1);
    }
    return crc;
}
static void make_frame(uint8_t frame[26])
{
    unsigned channel, bit;
    memset(frame, 0, 26u);
    frame[0] = 0xc8u; frame[1] = 24u; frame[2] = 0x16u;
    for (channel = 0u; channel < 16u; ++channel) {
        uint16_t value = channel == 2u ? 172u : 992u;
        for (bit = 0u; bit < 11u; ++bit)
            if (value & (1u << bit)) frame[3u + (channel * 11u + bit) / 8u] |= (uint8_t)(1u << ((channel * 11u + bit) % 8u));
    }
    frame[25] = crc8(frame + 2u, 23u);
}
static void feed(const uint8_t *bytes, unsigned count, uint8_t idle)
{
    unsigned i;
    for (i = 0u; i < count; ++i) {
        DMA_Channel_TypeDef *dma = DMA1_Channel6;
        assert(dma->enabled);
        dma->memory[dma->size - dma->remaining] = bytes[i];
        --dma->remaining;
        if (dma->remaining == dma->size / 2u) {
            fake_dma_flags |= DMA1_FLAG_HT6; DMA1_Channel6_IRQHandler();
        }
        if (dma->remaining == 0u) {
            dma->remaining = dma->size;
            fake_dma_flags |= DMA1_FLAG_TC6; DMA1_Channel6_IRQHandler();
        }
    }
    if (idle) { USART2->SR = USART_FLAG_IDLE; USART2_IRQHandler(); USART2->SR = 0u; }
}
static void service(void) { unsigned i; for (i = 0u; i < 4u; ++i) crsf_process(); }
int main(void)
{
    uint8_t frame[26], noise[600];
    crsf_init(); make_frame(frame); memset(noise, 0, sizeof(noise));
    feed(frame, 11u, 1u); service(); assert(crsf_valid_frame_count == 0u);
    feed(frame + 11u, 15u, 1u);
    assert(crsf_valid_frame_count == 0u); /* ISR只搬运通知，不解析 */
    service(); assert(crsf_valid_frame_count == 1u && rc_channels[2] == 1000);
    frame[25] ^= 1u; feed(frame, sizeof(frame), 1u); service();
    assert(crsf_valid_frame_count == 1u && crsf_crc_error_count == 1u); frame[25] ^= 1u;
    /* 无IDLE连续流仅靠HT/TC也必须触发消费，跨环形边界保留半帧。 */
    feed(noise, 200u, 0u); feed(frame, sizeof(frame), 0u); service();
    assert(crsf_valid_frame_count == 2u);
    feed(noise, 220u, 1u); service(); feed(frame, sizeof(frame), 1u); service();
    assert(crsf_valid_frame_count == 3u);
    feed(frame, 10u, 1u); service(); feed(noise, sizeof(noise), 1u); service();
    assert(crsf_get_rx_overruns() == 1u);
    feed(frame, sizeof(frame), 1u); service(); assert(crsf_valid_frame_count == 4u);
    feed(frame, 10u, 1u); service();
    fake_dma_flags |= DMA1_FLAG_TE6; DMA1_Channel6_IRQHandler(); service();
    feed(frame, sizeof(frame), 1u); service(); assert(crsf_valid_frame_count == 5u);
    puts("CRSF DMA stream tests passed");
    return 0;
}
