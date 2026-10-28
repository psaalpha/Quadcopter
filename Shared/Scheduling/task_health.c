#include "task_health.h"
uint8_t task_health_all_fresh(uint32_t now, uint32_t service,
                              uint32_t background, uint32_t limit_ms)
{
    return (uint8_t)((uint32_t)(now - service) < limit_ms &&
                     (uint32_t)(now - background) < limit_ms);
}
