#ifndef SLAVE_MCU_DRIVERS_COMMUNICATION_MASTER_LINK_H
#define SLAVE_MCU_DRIVERS_COMMUNICATION_MASTER_LINK_H
#include "inter_mcu_protocol.h"
typedef struct {
    uint32_t sent;
    uint32_t skipped_busy;
    uint32_t dma_errors;
    uint32_t encode_errors;
} master_link_stats_t;
void master_link_init(void);
/* Main loop only; DMA owns a private frame until completion is consumed. */
void master_link_process(uint32_t now_ms);
uint8_t master_link_send(const inter_mcu_sensor_data_t *packet, uint32_t now_ms);
uint8_t master_link_is_busy(void);
void master_link_get_stats(master_link_stats_t *stats);
#endif
