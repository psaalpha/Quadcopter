#include "rtos_support.h"
static TaskHandle_t volatile service_task;
static volatile uint8_t service_started;

void rtos_service_started(TaskHandle_t handle)
{
    /* 业务任务已开始执行后才能给它通知。外设初始化时到来的 IRQ
     * 仍保留驱动 pending 标志，首个 TIM2 tick 会重新唤醒消费者。
     */
    service_task = handle;
    service_started = 1u;
}
void rtos_notify_service_from_isr(uint32_t events)
{
    BaseType_t higher_priority_woken = pdFALSE;
    if (service_started == 0u) return;
    /* eSetBits 合并事件，不承载字节，也不计量每一次边沿。
     * 字节由 DMA ring 保留；周期次数/超期由 periodic_tasks 保留。
     * 先修改业务 pending 再通知；消费者醒来先取事件再执行业务。
     */
    xTaskNotifyFromISR(service_task, events, eSetBits, &higher_priority_woken);
    /* 这一步仅挂起 PendSV，不在这里执行任务。退出 ISR 后，
     * 若控制任务比被打断的显示/蓝牙任务优先级高，才立即切换过去。
     */
    portYIELD_FROM_ISR(higher_priority_woken);
}
void rtos_fault_stop(void)
{
    /* 不打印、不申请内存、不再喂狗。故障时即使当前任务栈已损坏，
     * 也尽可能先写安全硬件输出，随后由独立看门狗复位。
     */
    __disable_irq();
    rtos_board_emergency_stop();
    for (;;) { }
}
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    rtos_fault_stop();
}
