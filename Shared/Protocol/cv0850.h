#ifndef SHARED_PROTOCOL_CV0850_H
#define SHARED_PROTOCOL_CV0850_H
#include <stdint.h>
#define CV0850_FLOW 1u
#define CV0850_RANGE 2u
/* Native CV0850 UART and MSPv2/INAV output. Angular increments are 1e-4 rad. */
typedef struct {
    uint8_t frame[20], length, needed;
    uint8_t valid, quality, version;
    uint16_t distance_mm;
    int32_t delta_x, delta_y;
    uint32_t integration_us;
    uint32_t frames, checksum_errors, format_errors;
} cv0850_t;
void cv0850_init(cv0850_t *sensor);
uint8_t cv0850_feed(cv0850_t *sensor, uint8_t byte);
uint8_t cv0850_crc8(const uint8_t *data, uint8_t count);
#endif
