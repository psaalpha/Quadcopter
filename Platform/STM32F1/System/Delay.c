#include "stm32f10x.h"
#include "Delay.h"
#define DELAY_DWT_CTRL   (*((volatile uint32_t *)0xE0001000u))
#define DELAY_DWT_CYCCNT (*((volatile uint32_t *)0xE0001004u))
/* SysTick 已交给 FreeRTOS，驱动不能修改它的 LOAD/VAL/CTRL。
 * CYCCNT 是自由运行计数器：两个任务延时可交叠，各自保留自己的起点。
 * 中断抢占只会延长本次等待，不会让 I2C 低于最小脉宽；不是精密波形发生器。
 * 1ms 分块避免 32 位乘法/回绕歧义；不复位 CYCCNT，避免破坏毫秒时钟。
 */
void Delay_us(uint32_t us)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DELAY_DWT_CTRL |= 1u;
    while (us != 0u) {
        uint32_t part = us > 1000u ? 1000u : us;
        uint32_t cycles = (SystemCoreClock / 1000000u) * part;
        uint32_t start = DELAY_DWT_CYCCNT;
        while ((uint32_t)(DELAY_DWT_CYCCNT - start) < cycles) { }
        us -= part;
    }
}
/* 毫秒长等待只用于调度器启动前的设备初始化，运行期保持原有非阻塞状态机。
 * 不在通用延时中擅自 vTaskDelay：它也会被软件 I2C、Flash 等底层调用。
 */
void Delay_ms(uint32_t ms) { while (ms-- != 0u) Delay_us(1000u); }
void Delay_s(uint32_t seconds) { while (seconds-- != 0u) Delay_ms(1000u); }
