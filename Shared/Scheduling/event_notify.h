#ifndef SHARED_SCHEDULING_EVENT_NOTIFY_H
#define SHARED_SCHEDULING_EVENT_NOTIFY_H
#ifdef QUADCOPTER_FREERTOS
#include "rtos_support.h"
#else
#define RTOS_EVENT_IO 2u
#define RTOS_EVENT_TICK 1u
#define rtos_notify_service_from_isr(events) ((void)0)
#endif
#endif
