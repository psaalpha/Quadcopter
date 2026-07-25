#ifndef __CRSF_H
#define __CRSF_H

#include "stm32f10x.h"

/* ============================================
 * CRSF Protocol Constants
 * ============================================ */
#define CRSF_SYNC_BYTE_RC          0xC8u   /* RC channels from receiver */
#define CRSF_SYNC_BYTE_TLM         0xEEu   /* Telemetry from flight controller */
#define CRSF_TYPE_RC_CHANNELS      0x16u   /* RC channels packed (11-bit) */
#define CRSF_TYPE_LINK_STATS       0x14u   /* Link statistics */
#define CRSF_TYPE_ATTITUDE         0x1Eu   /* Attitude data */
#define CRSF_TYPE_BATTERY          0x08u   /* Battery sensor */
#define CRSF_TYPE_GPS              0x02u   /* GPS data */
#define CRSF_TYPE_HEARTBEAT        0x0Bu   /* Heartbeat */

#define CRSF_MAX_FRAME_LEN         64u     /* Max frame payload length */
#define CRSF_RC_CHANNELS_PAYLOAD   22u     /* 16 channels × 11 bits = 22 bytes */
#define CRSF_DMA_BUF_SIZE          128u    /* DMA ring buffer size */

#define CRSF_RC_CH_MIN             172u    /* ~1000us: 172 = 988us */
#define CRSF_RC_CH_MID             992u    /* ~1500us */
#define CRSF_RC_CH_MAX             1811u   /* ~2000us: 1811 = 2012us */
#define CRSF_RC_OUT_MIN            1000u
#define CRSF_RC_OUT_MAX            2000u

/* ============================================
 * Public Variables
 * ============================================ */
extern int16_t rcChannels[16];           /* Decoded RC channels (1000~2000) */
extern volatile uint8_t crsf_frame_received; /* Flag: valid frame decoded */
extern volatile uint32_t crsf_valid_frame_count;
extern volatile uint32_t crsf_crc_error_count;

/* ============================================
 * Public API
 * ============================================ */
void CRSF_Init(void);
void CRSF_Process(void);

#endif /* __CRSF_H */
