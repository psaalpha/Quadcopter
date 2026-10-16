#ifndef TASK_HEALTH_H
#define TASK_HEALTH_H
#include <stdint.h>
/* 纯逻辑喂狗门限。只能由监督者读取同一时刻的两个心跳快照。 */
uint8_t task_health_all_fresh(uint32_t now, uint32_t service,
                              uint32_t background, uint32_t limit_ms);
#endif
