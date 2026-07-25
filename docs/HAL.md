# HAL 抽象设计

完整 UART、I2C、SPI、PWM、TIMER 分层与迁移规则见
[HAL 总体设计](HAL_DESIGN.md)。本文保留首个 GPIO/LED 迁移案例。

## 1. 目的

HAL（Hardware Abstraction Layer）用于隔离“设备行为”和“芯片访问方式”。
它不是 STM32Cube HAL，也不替换当前 SPL；它是一组项目自有的小型接口，
由 STM32F1 BSP、电脑端 Fake 或未来其他平台分别实现。

首个接口只覆盖 GPIO 输出，先验证分层方式，再逐步增加 SPI、I2C、UART、
时间和临界区。

## 2. 当前调用链

```text
Master application
        |
legacy LED1_ON / LED2_ON / LED3_ON
        |
BoardLed API (board identity)
        |
StatusLed driver (active level and health)
        |
HalGpio interface
        |
Stm32f1GpioOutput adapter
        |
STM32F1 SPL GPIO
```

电脑端测试替换最后两层：

```text
StatusLed driver -> HalGpio -> FakeHalGpio
```

因此状态灯的 active-low 语义、错误处理和状态转换不需要板卡就能验证。

## 3. 接口文件

| 层 | 文件 | 职责 |
|---|---|---|
| HAL 契约 | `Shared/HAL/hal_gpio.h` | 定义电平、操作表、实例和状态 |
| HAL 防御层 | `Shared/HAL/hal_gpio.c` | 参数检查和操作分发 |
| 通用设备驱动 | `Shared/Drivers/status_led.*` | 亮灭语义、active level、健康状态 |
| STM32F1 适配 | `Master_MCU/BSP/stm32f1_gpio_hal.*` | 把 SPL GPIO 绑定到 HAL |
| 板级设备 | `Master_MCU/Hardware/LED.*` | 固定 PC13、PA0、PA5 的板级身份 |
| 测试替身 | `tests/host/fakes/fake_hal_gpio.*` | 记录操作并注入下一次失败 |

## 4. GPIO HAL 契约

`HalGpio` 由上下文指针和只读操作表组成：

```c
typedef struct
{
    void *context;
    const HalGpioOps *ops;
} HalGpio;
```

规则：

- HAL 实例由 BSP 或测试代码静态持有；
- Device Driver 不解释 `context`；
- 不使用动态内存；
- `write` 是 GPIO 输出的必选操作；
- `read` 是可选操作，缺失时返回 `HAL_STATUS_UNSUPPORTED`；
- HAL 返回硬件访问结果，Device Driver 映射为 `DriverStatus`；
- HAL 不包含 LED、遥控、传感器等业务含义。

## 5. 状态灯迁移

三路状态灯仍保持：

| BoardLedId | 引脚 | 有效电平 | 兼容接口 |
|---|---|---|---|
| `BOARD_LED_STATUS` | PC13 | 低 | `LED1_ON/OFF` |
| `BOARD_LED_AUXILIARY_1` | PA0 | 低 | `LED2_ON/OFF` |
| `BOARD_LED_AUXILIARY_2` | PA5 | 低 | `LED3_ON/OFF` |

新初始化会明确把三路输出设置为 OFF。原实现只显式关闭 PA0 和 PA5；
PC13 依赖复位状态。明确初始电平可以消除启动阶段的不确定指示，不影响
飞控、安全状态和电机输出。

## 6. 错误与健康状态

`StatusLed` 使用共享 `DriverHealth`：

- 初始化失败进入 `DRIVER_STATE_FAULT`；
- 运行期写入失败进入 `DRIVER_STATE_DEGRADED`；
- 下一次成功写入恢复 `DRIVER_STATE_READY`；
- 错误次数和连续错误次数保持可查询；
- 板级接口可通过 `BoardLed_GetHealth()` 暴露诊断信息。

兼容 `void` API 会忽略返回值，只用于未迁移调用方。新增代码必须优先调用
`BoardLed_Init()` 和 `BoardLed_Set()` 并处理 `DriverStatus`。

## 7. 后续 HAL 顺序

按风险从低到高：

1. GPIO input/output；
2. 单调时间和延时；
3. UART 非阻塞发送与接收；
4. SPI transaction；
5. I2C transaction；
6. 临界区和 RTOS 同步原语；
7. Flash/NVM 后端。

每次只迁移一个真实设备，并保留旧接口直到台架对比完成。
