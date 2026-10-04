#ifndef SLAVE_LINK_H
#define SLAVE_LINK_H
#include "inter_mcu_protocol.h"
typedef struct {
    uint32_t sent;
    uint32_t skipped_busy;
    uint32_t dma_errors;
    uint32_t encode_errors;
} SlaveLinkStats;
void SlaveLink_Init(void);
/* Main loop only; DMA owns a private frame until completion is consumed. */
void SlaveLink_Process(uint32_t now_ms);
uint8_t SlaveLink_Send(const InterMcuSensorData *packet, uint32_t now_ms);
uint8_t SlaveLink_IsBusy(void);
void SlaveLink_GetStats(SlaveLinkStats *stats);
#endif
