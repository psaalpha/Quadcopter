#ifndef SHARED_SCHEDULING_EVENT_NOTIFY_H
#define SHARED_SCHEDULING_EVENT_NOTIFY_H
/* The baremetal loop polls driver pending flags; no kernel is required. */
#define RTOS_EVENT_TICK 1u
#define RTOS_EVENT_IO 2u
#define rtos_notify_service_from_isr(events) ((void)(events))
#endif
