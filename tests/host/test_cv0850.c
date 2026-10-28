#include "cv0850.h"
#include <assert.h>
#include <string.h>
static uint8_t feed(cv0850_t *s,const uint8_t *p,unsigned n)
{ uint8_t events=0;unsigned i;for(i=0;i<n;i++) events|=cv0850_feed(s,p[i]);return events; }
int main(void)
{
    cv0850_t s;
    uint8_t native[19]={0xDF,0x15,0,0x55,1,12,0xE8,3,80,0,100,0,0x9C,0xFF,0x20,0x4E,245,1,0};
    uint8_t msp[18]={'$','X','<',0,2,0x1F,9,0,245,100,0,0,0,0x9C,0xFF,0xFF,0xFF,0};
    uint8_t range[14]={'$','X','<',0,1,0x1F,5,0,80,0xE8,3,0,0,0};
    unsigned i;uint8_t sum=0;
    cv0850_init(&s);
    for(i=0;i<18;i++) sum=(uint8_t)(sum+native[i]);
    native[18]=sum;
    assert(feed(&s,native,8)==0);assert(feed(&s,native+8,11)==3);
    assert(s.valid && s.delta_x==100 && s.delta_y==-100 && s.distance_mm==1000 && s.integration_us==20000);
    native[11]^=1;assert(feed(&s,native,19)==0 && s.checksum_errors==1);native[11]^=1;
    msp[17]=cv0850_crc8(msp+3,14);range[13]=cv0850_crc8(range+3,10);
    assert(feed(&s,range,14)==CV0850_RANGE);assert(s.distance_mm==1000);
    assert(feed(&s,msp,18)==CV0850_FLOW);assert(s.valid && s.delta_y==-100);
    msp[5]=0;assert(feed(&s,msp,18)==0);assert(feed(&s,native,19)==3);
    native[16]=0;sum=0;for(i=0;i<18;i++)sum=(uint8_t)(sum+native[i]);native[18]=sum;
    assert(feed(&s,native,19)==3 && !s.valid);
    return 0;
}
