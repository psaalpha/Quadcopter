#include "flash_config.h"
#include "config_journal.h"
#include "stm32f10x.h"
#include "stm32f10x_flash.h"
#include <string.h>
#define FIRST_PAGE 0x0800F800u
static volatile uint8_t busy;
static int read_bytes(void *unused,unsigned slot,unsigned offset,void *out,unsigned length)
{
    (void)unused;
    if(slot>1u || offset+length>1024u)return 0;
    memcpy(out,(const void *)(FIRST_PAGE+slot*1024u+offset),length);return 1;
}
static int erase_page(void *unused,unsigned slot)
{(void)unused;return slot<2u && FLASH_ErasePage(FIRST_PAGE+slot*1024u)==FLASH_COMPLETE;}
static int program_bytes(void *unused,unsigned slot,unsigned offset,const void *data,unsigned length)
{
    const uint8_t *p=(const uint8_t *)data;unsigned i;(void)unused;
    if(slot>1u || offset+length>1024u || (offset&1u) || (length&1u))return 0;
    for(i=0;i<length;i+=2u)if(FLASH_ProgramHalfWord(FIRST_PAGE+slot*1024u+offset+i,
        (uint16_t)((uint16_t)p[i]|((uint16_t)p[i+1u]<<8)))!=FLASH_COMPLETE)return 0;
    return 1;
}
static const config_backend_t backend={0,read_bytes,erase_page,program_bytes};
int flash_config_load(uint16_t schema,void *out,unsigned length)
{return config_journal_load(&backend,schema,out,length);}
int flash_config_save(uint16_t schema,const void *data,unsigned length)
{
    int ok;uint32_t mask=__get_PRIMASK();busy=1u;__disable_irq();
    FLASH_Unlock();FLASH_ClearFlag(FLASH_FLAG_EOP|FLASH_FLAG_PGERR|FLASH_FLAG_WRPRTERR);
    ok=config_journal_save(&backend,schema,data,length);
    FLASH_Lock();busy=0u;__set_PRIMASK(mask);return ok;
}
uint8_t flash_config_busy(void){return busy;}
