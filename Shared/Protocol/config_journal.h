#ifndef SHARED_PROTOCOL_CONFIG_JOURNAL_H
#define SHARED_PROTOCOL_CONFIG_JOURNAL_H
#include <stdint.h>
#define CONFIG_JOURNAL_MAX 256u
typedef struct {
    void *context;
    int (*read)(void *,unsigned,unsigned,void *,unsigned);
    int (*erase)(void *,unsigned);
    int (*program)(void *,unsigned,unsigned,const void *,unsigned);
} config_backend_t;
int config_journal_load(const config_backend_t *b,uint16_t schema,void *data,unsigned length);
int config_journal_save(const config_backend_t *b,uint16_t schema,const void *data,unsigned length);
#endif
