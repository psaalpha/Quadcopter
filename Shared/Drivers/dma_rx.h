#ifndef SHARED_DRIVERS_DMA_RX_H
#define SHARED_DRIVERS_DMA_RX_H

#include "stm32f10x.h"

/* One producer (DMA) and one consumer (main loop). ISR code only records
 * notifications and complete wraps; it never walks the byte buffer.
 * Size must be a power of two, so uint32_t counter rollover keeps indexing.
 */
typedef struct {
    DMA_Channel_TypeDef *channel;
    volatile uint8_t *buffer;
    uint16_t size;
    uint32_t half_flag;
    uint32_t full_flag;
    volatile uint32_t wraps;
    volatile uint8_t pending;
    uint32_t consumed;
    uint32_t overruns;
} dma_rx_t;

void dma_rx_init(dma_rx_t *rx, DMA_Channel_TypeDef *channel,
                volatile uint8_t *buffer, uint16_t size,
                uint32_t half_flag, uint32_t full_flag);
void dma_rx_notify_from_isr(dma_rx_t *rx);
void dma_rx_on_dma_interrupt(dma_rx_t *rx);
/* Main loop only. Copies a bounded batch before parsing; dropped is set
 * when unread bytes were overwritten and the protocol parser must reset.
 */
uint16_t dma_rx_read(dma_rx_t *rx, uint8_t *bytes, uint16_t capacity,
                    uint8_t *dropped);

#endif
