#ifndef SLAVE_MCU_APP_SLAVE_SCHEDULER_H
#define SLAVE_MCU_APP_SLAVE_SCHEDULER_H
#include "stm32f10x.h"
typedef enum {
    SLAVE_TASK_SENSORS = 0,
    SLAVE_TASK_DISPLAY,
    SLAVE_TASK_COUNT
} slave_task_id_t;
void slave_scheduler_init(void);
void slave_scheduler_tick_from_isr(uint32_t elapsed_ms);
uint32_t slave_scheduler_now_ms(void);
uint8_t slave_scheduler_take(slave_task_id_t task);
uint32_t slave_scheduler_get_overruns(slave_task_id_t task);
#endif
