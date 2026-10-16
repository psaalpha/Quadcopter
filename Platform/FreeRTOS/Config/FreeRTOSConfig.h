#ifndef QUADCOPTER_FREERTOS_CONFIG_H
#define QUADCOPTER_FREERTOS_CONFIG_H
#include <stdint.h>
extern uint32_t SystemCoreClock;
void rtos_fault_stop(void);

/* 两颗 F103 都是单核：所谓并发，是内核保存/恢复任务栈后轮流执行。
 * 优先级数字越大越先运行；最高业务任务必须尽快阻塞，不能 while(1) 空转。
 * SysTick 只服务内核的 1ms tick，TIM2 保留原周期发布和真实毫秒记账。
 */
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   0
#define configCPU_CLOCK_HZ                       SystemCoreClock
#define configTICK_RATE_HZ                       1000u
#define configMAX_PRIORITIES                     6u
#define configMINIMAL_STACK_SIZE                 128u
#define configMAX_TASK_NAME_LEN                  12u
#define configTICK_TYPE_WIDTH_IN_BITS            TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                  0
#define configUSE_TICKLESS_IDLE                  0
#define configUSE_TASK_NOTIFICATIONS             1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    1

/* 全部静态分配：TCB、栈、队列在链接时计入 ZI RAM，运行中不 malloc。
 * 不编译 heap_1/2/3/4/5.c；C 库启动堆与 RTOS 任务栈是不同的资源。
 */
#define configSUPPORT_STATIC_ALLOCATION          1
#define configSUPPORT_DYNAMIC_ALLOCATION         0
#define configKERNEL_PROVIDED_STATIC_MEMORY      1
#define configCHECK_FOR_STACK_OVERFLOW           2
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configUSE_TIMERS                         0
/* V11.1.0 的内核静态回调实现即使关闭timer也需该宏；未创建timer任务。 */
#define configTIMER_TASK_STACK_DEPTH             128
#define configUSE_MUTEXES                        0
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES            0
#define configQUEUE_REGISTRY_SIZE                0
#define configUSE_TRACE_FACILITY                 0
#define configGENERATE_RUN_TIME_STATS            0
#define configUSE_CO_ROUTINES                    0
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_uxTaskGetStackHighWaterMark       1
#define INCLUDE_xTaskGetSchedulerState           1

/* STM32F103 实现 4 位中断优先级，SPL 使用未移位值 0..15。
 * Group_4 = 4 位抢占、0 位子优先级。中断数字越小越紧急，和任务相反。
 * 只有数值 >=5 的中断可调用 FromISR API；所有业务 IRQ 设置为 6/7/8。
 * BASEPRI=0x50 的临界区屏蔽这些 IRQ；0..4 预留且不能调用内核。
 */
#define configPRIO_BITS                         4u
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15u
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5u
#define configKERNEL_INTERRUPT_PRIORITY          (15u << 4u)
/* ARMCC内联汇编也读取本宏，必须使用汇编能解析的整数字面量。 */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY     0x50
#define configASSERT(x) do { if (!(x)) rtos_fault_stop(); } while (0)

/* 向量表直接指向官方移植层；不能再保留 ST 模板里的三个空函数。
 * SVC 启动首个任务；PendSV 保存 R4-R11/PSP 并选择下一任务；
 * 异常入栈自动保存 R0-R3/R12/LR/PC/xPSR。任务使用 PSP，中断使用 MSP。
 */
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler
#endif
