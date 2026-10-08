#include "dma_rx.h"

void dma_rx_init(dma_rx_t *rx, DMA_Channel_TypeDef *channel,
                volatile uint8_t *buffer, uint16_t size,
                uint32_t half_flag, uint32_t full_flag)
{
    rx->channel = channel;
    rx->buffer = buffer;
    rx->size = size;
    rx->half_flag = half_flag;
    rx->full_flag = full_flag;
    rx->wraps = 0u;
    rx->pending = 0u;
    rx->consumed = 0u;
    rx->overruns = 0u;
}

void dma_rx_notify_from_isr(dma_rx_t *rx)
{
    rx->pending = 1u;
}

void dma_rx_on_dma_interrupt(dma_rx_t *rx)
{
    if (DMA_GetFlagStatus(rx->half_flag) != RESET) {
        DMA_ClearFlag(rx->half_flag);
        rx->pending = 1u;
    }
    if (DMA_GetFlagStatus(rx->full_flag) != RESET) {
        DMA_ClearFlag(rx->full_flag);
        rx->wraps++;
        rx->pending = 1u;
    }
}

/* Called with interrupts masked. Handle a TC that arrived before its ISR
 * ran, then sample CNDTR again. DMA remains enabled throughout.
 * A TC flag can count only one wrap: interrupt masking must stay shorter
 * than one full buffer time (22ms at 115200, 67ms at 38400 for 256 bytes).
 */
static uint32_t dma_rx_produced(dma_rx_t *rx)
{
    uint32_t remaining = DMA_GetCurrDataCounter(rx->channel);
    if (DMA_GetFlagStatus(rx->full_flag) != RESET) {
        DMA_ClearFlag(rx->full_flag);
        rx->wraps++;
        remaining = DMA_GetCurrDataCounter(rx->channel);
    }
    return rx->wraps * rx->size + (rx->size - remaining);
}

uint16_t dma_rx_read(dma_rx_t *rx, uint8_t *bytes, uint16_t capacity,
                    uint8_t *dropped)
{
    uint32_t interrupt_mask;
    uint32_t produced;
    uint32_t after_copy;
    uint32_t available;
    uint16_t count;
    uint16_t index;

    *dropped = 0u;
    interrupt_mask = __get_PRIMASK();
    __disable_irq();
    if (rx->pending == 0u) {
        __set_PRIMASK(interrupt_mask);
        return 0u;
    }
    rx->pending = 0u; /* Take event before processing; new ISR events survive. */
    produced = dma_rx_produced(rx);
    __set_PRIMASK(interrupt_mask);

    available = (uint32_t)(produced - rx->consumed);
    if (available >= rx->size) {
        rx->consumed = produced;
        rx->overruns++;
        *dropped = 1u;
        return 0u;
    }
    count = (uint16_t)((available < capacity) ? available : capacity);
    for (index = 0u; index < count; ++index) {
        bytes[index] = rx->buffer[(rx->consumed + index) & (rx->size - 1u)];
    }

    /* Recheck after copying: DMA may have overwritten an unread slot
     * during the copy. Discard the whole uncertain batch, never decode it.
     */
    interrupt_mask = __get_PRIMASK();
    __disable_irq();
    after_copy = dma_rx_produced(rx);
    if ((uint32_t)(after_copy - rx->consumed) >= rx->size) {
        rx->consumed = after_copy;
        rx->overruns++;
        *dropped = 1u;
        count = 0u;
    } else {
        rx->consumed += count;
    }
    if (after_copy != rx->consumed) {
        rx->pending = 1u; /* More data remains; continue next main-loop turn. */
    }
    __set_PRIMASK(interrupt_mask);
    return count;
}
