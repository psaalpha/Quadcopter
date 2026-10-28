#include "stm32f10x.h"
#include "Delay.h"
#include "flight_controller.h"
#include "app_scheduler.h"
#include "control_timers.h"
#include "millisecond_clock.h"
#include "bluetooth_serial.h"
#include "watchdog.h"
#include "task_health.h"
#include <string.h>
#ifdef QUADCOPTER_FREERTOS
#include "rtos_support.h"
#endif
volatile uint32_t system_time_ms;
static volatile uint32_t control_progress_ms;
static volatile uint8_t control_started;
volatile uint32_t master_max_step_cycles;
volatile uint32_t master_control_stack_free,master_background_stack_free;

void system_clock_configure(void)
{
	uint8_t retry = 0;

	/* 复位时钟配置 */
	RCC_DeInit();
	SystemCoreClockUpdate();

	/* 启动外部高速晶振 HSE */
	RCC_HSEConfig(RCC_HSE_ON);

	/* 等待 HSE 稳定，超时后重新拉起，降低长按复位后的起振风险 */
	while (RCC_WaitForHSEStartUp() != SUCCESS)
	{
		retry++;
		if(retry > 10)
		{
			RCC_HSEConfig(RCC_HSE_OFF);
			Delay_ms(10);
			RCC_HSEConfig(RCC_HSE_ON);
			retry = 0;
		}
	}

	/* 72MHz 下必须配置 Flash 预取和等待周期 */
	FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
	FLASH_SetLatency(FLASH_Latency_2);

	/* 总线分频：AHB=72MHz，APB1=36MHz，APB2=72MHz */
	RCC_HCLKConfig(RCC_SYSCLK_Div1);
	RCC_PCLK1Config(RCC_HCLK_Div2);
	RCC_PCLK2Config(RCC_HCLK_Div1);

	/* PLL = 8MHz * 9 = 72MHz */
	RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
	RCC_PLLCmd(ENABLE);

	/* 等待 PLL 锁定 */
	while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

	/* 切换系统时钟到 PLL */
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
	while (RCC_GetSYSCLKSource() != 0x08);
}


static void emergency_stop(void)
{
    __disable_irq();flight_controller_emergency_stop();for(;;){}
}
#ifdef QUADCOPTER_FREERTOS
static TaskHandle_t control_handle;
static StaticTask_t control_cb,background_cb;
static StackType_t control_stack[1024],background_stack[640];
static QueueHandle_t command_queue,response_queue,telemetry_queue;
static StaticQueue_t command_cb,response_cb,telemetry_cb;
static uint8_t command_storage[4u*sizeof(flight_command_t)],response_storage[4u*96u],telemetry_storage[128u];
static volatile uint32_t background_progress_ms;
void rtos_board_emergency_stop(void){flight_controller_emergency_stop();}
static void background_task(void *unused)
{
    char pending[128]={0};flight_command_t command;uint8_t have_command=0u;
    (void)unused;
    for(;;) {
        int result;
        bluetooth_serial_service_tx(system_time_ms);
        if(pending[0]==0) {
            if(xQueueReceive(response_queue,pending,0u)!=pdPASS)
                xQueueReceive(telemetry_queue,pending,0u);
        }
        if(pending[0] && bluetooth_serial_try_send((uint8_t *)pending,(uint16_t)strlen(pending)))pending[0]=0;
        if(!have_command && pending[0]==0) {
            result=bluetooth_serial_take_command(&command);
            if(result>0)have_command=1u;
            else if(result<0)strcpy(pending,"[error,bad_command]");
        }
        if(have_command && xQueueSend(command_queue,&command,0u)==pdPASS)have_command=0u;
        background_progress_ms=system_time_ms;
        master_background_stack_free=(uint32_t)uxTaskGetStackHighWaterMark(NULL);
        vTaskDelay(pdMS_TO_TICKS(1u));
    }
}
static void control_task(void *unused)
{
    unsigned page=0u;uint32_t stack_sample=0u;(void)unused;
    rtos_service_started(control_handle);control_progress_ms=system_time_ms;
    control_started=1u;board_control_timers_init();
    for(;;) {
        uint32_t events,start,duration;flight_command_t command;char response[128]={0};
        xTaskNotifyWait(0u,0xffffffffu,&events,portMAX_DELAY);(void)events;
        start=*((volatile uint32_t *)0xE0001004u);
        if(!flight_controller_maintenance() && app_scheduler_get_overrun_count(APP_TASK_IMU_UPDATE)!=0u)rtos_fault_stop();
        flight_controller_step();
        if(uxQueueSpacesAvailable(response_queue)!=0u && xQueueReceive(command_queue,&command,0u)==pdPASS) {
            flight_controller_command(&command,response,96u);
            xQueueSend(response_queue,response,0u);
        }
        if(app_scheduler_take(APP_TASK_TELEMETRY)) {
            flight_controller_telemetry(response,sizeof(response),page++);
            xQueueOverwrite(telemetry_queue,response);
        }
        control_progress_ms=system_time_ms;
        flight_controller_finish_maintenance();
        if(task_health_all_fresh(system_time_ms,control_progress_ms,background_progress_ms,100u))IWDG_ReloadCounter();
        if((uint32_t)(system_time_ms-stack_sample)>=1000u) {
            stack_sample=system_time_ms;master_control_stack_free=(uint32_t)uxTaskGetStackHighWaterMark(NULL);
        }
        duration=*((volatile uint32_t *)0xE0001004u)-start;
        if(duration>master_max_step_cycles)master_max_step_cycles=duration;
    }
}
#endif
int main(void)
{
    Delay_us(500);system_clock_configure();SystemCoreClockUpdate();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);app_scheduler_init();
    flight_controller_init();bluetooth_serial_init();watchdog_init();
    flight_controller_set_reset_flags(RCC->CSR & 0xfc000000u);RCC_ClearFlag();
#ifdef QUADCOPTER_FREERTOS
    command_queue=xQueueCreateStatic(4u,sizeof(flight_command_t),command_storage,&command_cb);
    response_queue=xQueueCreateStatic(4u,96u,response_storage,&response_cb);
    telemetry_queue=xQueueCreateStatic(1u,128u,telemetry_storage,&telemetry_cb);
    configASSERT(command_queue && response_queue && telemetry_queue);
    control_handle=xTaskCreateStatic(control_task,"control",1024u,NULL,5u,control_stack,&control_cb);
    configASSERT(control_handle && xTaskCreateStatic(background_task,"bluetooth",640u,NULL,2u,background_stack,&background_cb));
    vTaskStartScheduler();rtos_fault_stop();
#else
    {
        char pending[128]={0};unsigned page=0u;
        control_started=1u;board_control_timers_init();
        for(;;) {
            uint32_t start=*((volatile uint32_t *)0xE0001004u),duration;
            flight_command_t command;int result;
            flight_controller_step();bluetooth_serial_service_tx(system_time_ms);
            if(pending[0] && bluetooth_serial_try_send((uint8_t *)pending,(uint16_t)strlen(pending)))pending[0]=0;
            if(pending[0]==0) {
                result=bluetooth_serial_take_command(&command);
                if(result>0)flight_controller_command(&command,pending,sizeof(pending));
                else if(result<0)strcpy(pending,"[error,bad_command]");
                else if(app_scheduler_take(APP_TASK_TELEMETRY))flight_controller_telemetry(pending,sizeof(pending),page++);
            }
            control_progress_ms=system_time_ms;flight_controller_finish_maintenance();
            if(task_health_all_fresh(system_time_ms,flight_controller_last_imu_step(),control_progress_ms,20u))IWDG_ReloadCounter();
            duration=*((volatile uint32_t *)0xE0001004u)-start;
            if(duration>master_max_step_cycles)master_max_step_cycles=duration;
        }
    }
#endif
    return 0;
}
void TIM2_IRQHandler(void)
{
    if(TIM_GetITStatus(TIM2,TIM_IT_Update)!=RESET) {
        uint32_t elapsed;
        TIM_ClearITPendingBit(TIM2,TIM_IT_Update);elapsed=millisecond_clock_elapsed_ms();
        system_time_ms+=elapsed;app_scheduler_tick_from_isr(elapsed);
        if(control_started && !flight_controller_maintenance() && (uint32_t)(system_time_ms-control_progress_ms)>=10u)emergency_stop();
#ifdef QUADCOPTER_FREERTOS
        rtos_notify_service_from_isr(RTOS_EVENT_TICK);
#endif
    }
}
