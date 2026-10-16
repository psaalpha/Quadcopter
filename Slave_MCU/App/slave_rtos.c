#include "rtos_support.h"
#include "slave_rtos.h"
#include "slave_app.h"
#include "slave_board.h"
#include "slave_scheduler.h"
#include "task_health.h"
static StaticTask_t service_cb, display_cb;
static StackType_t service_stack[1024], display_stack[640];
static TaskHandle_t service_handle, display_handle;
static StaticQueue_t display_queue_cb;
static uint8_t display_storage[sizeof(slave_display_snapshot_t)];
static QueueHandle_t display_queue;
static volatile uint32_t service_progress_ms, display_progress_ms;
static uint32_t last_stack_sample_ms;
volatile uint32_t slave_max_step_cycles;
volatile UBaseType_t slave_service_stack_free, slave_display_stack_free;

void slave_rtos_submit_display(const slave_display_snapshot_t *snapshot)
{
    /* 长度1的最新值邮箱，内核在短临界区复制完整结构体。
     * 不传裸指针，不让显示任务持有传感器/I2C/Flash资源，因而无需mutex。
     * 若将来真的共享总线，需任务层互斥锁及优先级继承，不能靠volatile。
     */
    xQueueOverwrite(display_queue, snapshot);
}
static void display_task(void *argument)
{
    (void)argument;
    for (;;) {
        slave_display_snapshot_t snapshot;
        if (xQueueReceive(display_queue, &snapshot, 0u) == pdPASS)
            slave_app_display_step(&snapshot);
        else slave_app_display_step(NULL);
        display_progress_ms = slave_scheduler_now_ms();
        slave_display_stack_free = uxTaskGetStackHighWaterMark(NULL);
        vTaskDelay(pdMS_TO_TICKS(1u));
    }
}
static void service_task(void *argument)
{
    (void)argument;
    rtos_service_started(service_handle);
    slave_board_start_tick();
    for (;;) {
        uint32_t events, started, duration, now;
        xTaskNotifyWait(0u, 0xffffffffu, &events, portMAX_DELAY);
        (void)events;
        started = *((volatile uint32_t *)0xE0001004u);
        /* 单一所有者顺序调用原来的非阻塞业务：光流、输入、ADC、
         * 主从DMA回收、50ms传感器/发包、200ms显示快照。
         * QMC短I2C事务仍同步，Flash校准仍会暂停接收；均需实测最坏时间。
         */
        slave_app_process();
        now = slave_scheduler_now_ms();
        service_progress_ms = now;
        if (task_health_all_fresh(now, service_progress_ms, display_progress_ms, 250u))
            slave_board_feed_watchdog();
        if ((uint32_t)(now - last_stack_sample_ms) >= 1000u) {
            last_stack_sample_ms = now;
            slave_service_stack_free = uxTaskGetStackHighWaterMark(NULL);
        }
        duration = *((volatile uint32_t *)0xE0001004u) - started;
        if (duration > slave_max_step_cycles) slave_max_step_cycles = duration;
    }
}
void rtos_board_emergency_stop(void)
{
    /* 从控不直接驱动电机；故障时将舵机设为原协议的低电平命令位置。 */
    slave_board_set_servo(SLAVE_SERVO_LOW_US);
}
void slave_rtos_start(void)
{
    display_queue = xQueueCreateStatic(1u, sizeof(slave_display_snapshot_t),
                                       display_storage, &display_queue_cb);
    configASSERT(display_queue != NULL);
    /* 4=采集/通信，1=显示，0=idle。1024/640是栈字，不是字节。 */
    service_handle = xTaskCreateStatic(service_task, "sensors", 1024u, NULL, 4u,
                                       service_stack, &service_cb);
    display_handle = xTaskCreateStatic(display_task, "display", 640u, NULL, 1u,
                                       display_stack, &display_cb);
    configASSERT(service_handle != NULL && display_handle != NULL);
    vTaskStartScheduler();
    rtos_fault_stop();
}
