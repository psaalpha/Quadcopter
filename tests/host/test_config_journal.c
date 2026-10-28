#include "config_journal.h"
#include <assert.h>
#include <string.h>
static uint8_t pages[2][1024];static int budget;
static int read_bytes(void *c,unsigned slot,unsigned offset,void *out,unsigned n)
{(void)c;memcpy(out,pages[slot]+offset,n);return 1;}
static int erase(void *c,unsigned slot)
{(void)c;if(budget==0)return 0;budget--;memset(pages[slot],255,1024);return 1;}
static int program(void *c,unsigned slot,unsigned offset,const void *in,unsigned n)
{
    unsigned i;const uint8_t *p=in;(void)c;
    for(i=0;i<n;i+=2){if(budget==0)return 0;budget--;
        assert((pages[slot][offset+i]&p[i])==p[i]);pages[slot][offset+i]&=p[i];pages[slot][offset+i+1]&=p[i+1];}
    return 1;
}
int main(void)
{
    config_backend_t b={0,read_bytes,erase,program};
    uint8_t first[256],second[256],out[256],snapshot[2][1024];int cut;
    memset(pages,255,sizeof(pages));memset(first,17,sizeof(first));memset(second,34,sizeof(second));
    budget=1000;assert(!config_journal_load(&b,1,out,sizeof(first)));assert(config_journal_save(&b,1,first,sizeof(first)));
    memcpy(snapshot,pages,sizeof(pages));
    for(cut=0;cut<145;cut++) {
        memcpy(pages,snapshot,sizeof(pages));budget=cut;config_journal_save(&b,1,second,sizeof(first));
        assert(config_journal_load(&b,1,out,sizeof(first)));assert(!memcmp(out,first,sizeof(first))||!memcmp(out,second,sizeof(first)));
    }
    memcpy(pages,snapshot,sizeof(pages));budget=1000;assert(config_journal_save(&b,1,second,sizeof(first)));
    assert(config_journal_load(&b,1,out,sizeof(first))&&!memcmp(out,second,sizeof(first)));
    pages[1][20]^=1;assert(config_journal_load(&b,1,out,sizeof(first))&&!memcmp(out,first,sizeof(first)));
    assert(!config_journal_load(&b,2,out,sizeof(first)));return 0;
}
