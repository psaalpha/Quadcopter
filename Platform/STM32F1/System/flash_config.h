#ifndef PLATFORM_FLASH_CONFIG_H
#define PLATFORM_FLASH_CONFIG_H
#include <stdint.h>
/* Both boards reserve their final two 1KB pages. Firmware IROM ends at F800. */
int flash_config_load(uint16_t schema,void *data,unsigned length);
int flash_config_save(uint16_t schema,const void *data,unsigned length);
uint8_t flash_config_busy(void);
#endif
