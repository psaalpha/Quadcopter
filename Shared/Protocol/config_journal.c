#include "config_journal.h"
#include "inter_mcu_protocol.h"
#include <string.h>
#define MAGIC 0x51434647u
static void put32(uint8_t *p,uint32_t n)
{p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24);}
static uint32_t get32(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint16_t crc(const uint8_t *p,unsigned length)
{return inter_mcu_crc16_ccitt(p,(uint16_t)length);}
static int read_slot(const config_backend_t *b,unsigned slot,uint16_t schema,unsigned n,uint8_t *record)
{
    if(!b->read(b->context,slot,0,record,n+16u))return 0;
    return get32(record)==MAGIC && record[4]==(uint8_t)schema && record[5]==(uint8_t)(schema>>8) &&
        record[6]==(uint8_t)n && record[7]==(uint8_t)(n>>8) &&
        ((uint16_t)record[12]|((uint16_t)record[13]<<8))==crc(record+16,n) &&
        ((uint16_t)record[14]|((uint16_t)record[15]<<8))==crc(record+4,8u);
}
static int newest(const config_backend_t *b,uint16_t schema,unsigned n,uint8_t *record,uint32_t *generation)
{
    uint8_t other[CONFIG_JOURNAL_MAX+16u]={0};
    int a=read_slot(b,0u,schema,n,record),d=read_slot(b,1u,schema,n,other);
    uint32_t ga=get32(record+8),gb=get32(other+8);
    if(d && (!a || (gb!=ga && (uint32_t)(gb-ga)<0x80000000u))) {
        memcpy(record,other,n+16u);*generation=gb;return 1;
    }
    if(a){*generation=ga;return 0;}
    *generation=0u;return -1;
}
int config_journal_load(const config_backend_t *b,uint16_t schema,void *data,unsigned n)
{
    uint8_t record[CONFIG_JOURNAL_MAX+16u]={0};uint32_t generation;
    if(!b || !b->read || !data || n==0u || n>CONFIG_JOURNAL_MAX)return 0;
    if(newest(b,schema,n,record,&generation)<0)return 0;
    memcpy(data,record+16,n);return 1;
}
int config_journal_save(const config_backend_t *b,uint16_t schema,const void *data,unsigned n)
{
    uint8_t record[CONFIG_JOURNAL_MAX+16u]={0},verify[CONFIG_JOURNAL_MAX+16u];
    uint32_t generation;uint16_t c;int old;unsigned slot;
    if(!b || !b->read || !b->erase || !b->program || !data || n==0u || n>CONFIG_JOURNAL_MAX || (n&1u))return 0;
    old=newest(b,schema,n,record,&generation);slot=old==0?1u:0u;
    memset(record,0,sizeof(record));put32(record,MAGIC);
    record[4]=(uint8_t)schema;record[5]=(uint8_t)(schema>>8);
    record[6]=(uint8_t)n;record[7]=(uint8_t)(n>>8);put32(record+8,generation+1u);
    memcpy(record+16,data,n);c=crc(record+16,n);record[12]=(uint8_t)c;record[13]=(uint8_t)(c>>8);
    c=crc(record+4,8u);record[14]=(uint8_t)c;record[15]=(uint8_t)(c>>8);
    /* The previously committed page is untouched until its replacement is valid. */
    if(!b->erase(b->context,slot) || !b->program(b->context,slot,4u,record+4,n+12u) ||
       !b->program(b->context,slot,0u,record,4u))return 0;
    return read_slot(b,slot,schema,n,verify) && memcmp(record,verify,n+16u)==0;
}
