#ifndef QUADCOPTER_RTOS_SUPPORT_H
#define QUADCOPTER_RTOS_SUPPORT_H
#include "stm32f10x.h"
#ifdef QUADCOPTER_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#define RTOS_EVENT_TICK 1u
#define RTOS_EVENT_IO   2u
void rtos_service_started(TaskHandle_t handle);
void rtos_notify_service_from_isr(uint32_t events);
void rtos_fault_stop(void);
void rtos_board_emergency_stop(void);
#else
/* 寄存器模型回归仍可独立测试驱动，不把主机测试伪装成内核时序验证。 */
#define rtos_notify_service_from_isr(events) ((void)0)
#define RTOS_EVENT_TICK 1u
#define RTOS_EVENT_IO   2u
#endif
#endif
