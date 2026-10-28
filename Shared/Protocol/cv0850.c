#include "cv0850.h"
#include <string.h>
static uint16_t u16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }
static int32_t i32(const uint8_t *p)
{
    uint32_t u = (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
    int32_t value; memcpy(&value, &u, sizeof(value)); return value;
}
uint8_t cv0850_crc8(const uint8_t *p, uint8_t count)
{
    uint8_t crc=0u, i, bit;
    for(i=0u;i<count;i++) {
        crc ^= p[i];
        for(bit=0u;bit<8u;bit++) crc=(uint8_t)((crc & 0x80u) ? ((uint32_t)crc<<1)^0xD5u : (uint32_t)crc<<1);
    }
    return crc;
}
void cv0850_init(cv0850_t *s) { memset(s,0,sizeof(*s)); }
static void restart(cv0850_t *s, uint8_t byte)
{
    s->length=0u; s->needed=0u;
    if(byte==0xDFu || byte=='$') s->frame[s->length++]=byte;
}
uint8_t cv0850_feed(cv0850_t *s, uint8_t byte)
{
    uint8_t *f=s->frame, event=0u, sum=0u, i;
    if(s->length==0u) { restart(s,byte); return 0u; }
    f[s->length++]=byte;
    if(f[0]==0xDFu) {
        if((s->length==2u && byte!=0x15u) || (s->length==3u && byte!=0u) ||
           (s->length==4u && byte!=0x55u) ||
           (s->length==6u && byte!=12u)) { s->format_errors++; restart(s,byte); return 0u; }
        s->needed=19u;
    } else {
        if((s->length==2u && byte!='X') || (s->length==3u && byte!='<')) {
            s->format_errors++; restart(s,byte); return 0u;
        }
        if(s->length==8u) {
            uint16_t function=u16(f+4);
            if(f[3]!=0u || !((function==0x1F01u && u16(f+6)==5u) ||
                             (function==0x1F02u && u16(f+6)==9u))) {
                s->format_errors++; restart(s,byte); return 0u;
            }
            s->needed=(uint8_t)(9u+f[6]);
        }
    }
    if(s->needed==0u || s->length<s->needed) return 0u;
    if(f[0]==0xDFu) {
        for(i=0u;i<18u;i++) sum=(uint8_t)(sum+f[i]);
        if(sum!=f[18]) { s->checksum_errors++; restart(s,byte); return 0u; }
        s->distance_mm=u16(f+6); s->quality=f[8]; s->version=f[17];
        s->delta_x=(int16_t)u16(f+10); s->delta_y=(int16_t)u16(f+12);
        s->integration_us=u16(f+14);
        s->valid=(uint8_t)(f[16]==245u && s->integration_us>=100u && s->quality<=100u);
        event=CV0850_FLOW|CV0850_RANGE;
    } else {
        if(cv0850_crc8(f+3,(uint8_t)(s->needed-4u))!=f[s->needed-1u]) {
            s->checksum_errors++; restart(s,byte); return 0u;
        }
        if(f[4]==1u) {
            int32_t distance=i32(f+9);
            s->distance_mm=(uint16_t)((distance>=20 && distance<=8000)?distance:0);
            s->quality=f[8]; event=CV0850_RANGE;
        } else {
            s->delta_x=i32(f+9); s->delta_y=i32(f+13);
            s->valid=(uint8_t)(f[8]==245u); s->integration_us=20000u;
            event=CV0850_FLOW;
        }
    }
    if(s->distance_mm<20u || s->distance_mm>8000u) s->distance_mm=0u;
    s->frames++; s->length=0u; s->needed=0u;
    return event;
}
