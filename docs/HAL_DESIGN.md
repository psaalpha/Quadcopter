# HAL 总体设计

## 1. 目标架构

```text
Application
  飞行编排、安全策略、任务入口
        |
Middleware
  协议、参数、日志、设备驱动
        |
HAL contracts
  UART / I2C / SPI / GPIO / PWM / TIMER
        |
STM32 Platform adapters
  SPL、寄存器、DMA、IRQ、板级资源
```

新 Application 和 Middleware 代码禁止：

- 包含 `stm32f10x.h`；
- 直接调用 `GPIO_*`、`USART_*`、`DMA_*`、`TIM_*`；
- 假设某个固定 USART、DMA channel、GPIO 或 timer；
- 在设备驱动中配置 NVIC 和 RCC；
- 用业务含义命名 HAL，例如 `MotorUart` 或 `ImuTimer`。

历史 `Hardware` 驱动仍有直接 SPL 依赖，按一个设备一个提交迁移，不能为目录
整齐一次性重写。

## 2. 公共规则

### 2.1 静态实例

所有 HAL 都由：

```c
typedef struct
{
    void *context;
    const XxxOps *ops;
} HalXxx;
```

组成。BSP 静态保存具体 context，操作表为 `const`。不使用堆。

### 2.2 状态

`hal_status.h` 是所有接口的唯一状态来源：

- `OK`；
- `INVALID_ARGUMENT`；
- `NOT_INITIALIZED`；
- `BUSY`；
- `IO_ERROR`；
- `UNSUPPORTED`；
- `OUT_OF_RANGE`；
- `TIMEOUT`。

异步操作使用 `HalTransferState`：`IDLE/BUSY/COMPLETE/ERROR`。

### 2.3 非阻塞模型

总线和 UART 使用：

```text
Start(...)
  -> BUSY or accepted

GetState(...)
  -> IDLE / BUSY / COMPLETE / ERROR

Cancel(...)
  -> optional
```

上层不得在 500 Hz/200 Hz 任务中轮询等待完成。DMA/IRQ 完成后由任务读取状态
或消费事件。

### 2.4 所有权

- Start 成功到完成前，传入 buffer 所有权属于 HAL；
- 上层不得修改 TX buffer 或释放 RX buffer；
- 同一实例一次只允许一个 in-flight transfer，除非接口文档明确支持队列；
- 回调运行上下文必须由 adapter 文档说明；
- cancel 后 buffer 何时可复用由 adapter 明确。

## 3. UART HAL

用途：CRSF、主从链路、蓝牙和未来地面站。

接口：

- `HalUart_StartTransmit()`：启动 DMA/IRQ 发送；
- `HalUart_Read()`：非阻塞读取已接收字节；
- `HalUart_GetTransmitState()`；
- `HalUart_AbortTransmit()`：可选。

约束：

- Start 不等待全部字节发完；
- 接收推荐 DMA ring 或 ISR ring；
- overflow、framing、parity 和 overrun 必须由 adapter 计数；
- 协议解析位于 Middleware，不进入 UART HAL。

## 4. I2C HAL

`HalI2cTransfer` 支持：

- 7-bit address；
- write-only；
- read-only；
- write-then-read repeated-start。

接口使用 start/poll/cancel。设备驱动负责寄存器语义，HAL 只负责 transaction。
adapter 必须提供总线 busy、NACK、arbitration、timeout 和 bus recovery 策略。

## 5. SPI HAL

`HalSpiTransfer` 定义：

- TX 指针；
- RX 指针；
- 固定长度；
- 板级 chip-select ID。

chip select 映射属于 BSP。设备驱动不能直接拉 GPIO。SPI mode、bit order 和
最大频率由 adapter 实例配置，不能由每次 transaction 隐式改变。

## 6. PWM HAL

PWM 使用 `pulse_ticks`，而不是百分比或裸寄存器：

- adapter 提供每个 channel 的 min/max ticks；
- wrapper 在写硬件前做范围检查；
- enable 与 set pulse 分开；
- tick 频率、周期和 channel 映射属于 BSP。

当前电机 `PWM4` 尚未迁移。迁移时必须保持 timer prescaler、ARR、CCR 映射和
写入时点完全一致，并进行示波器对比。

## 7. TIMER HAL

当前基础接口：

- start；
- stop；
- counter ticks；
- frequency Hz。

上层根据明确的 frequency 换算时间，不能假设 1 tick = 1 us。未来 callback、
capture/compare 和 monotonic clock 应分别扩展，不能把所有 timer 功能堆入
一个万能接口。

## 8. STM32 Platform Adapter 规则

adapter 可以：

- 包含 STM32/SPL 头文件；
- 配置 RCC、GPIO、DMA、NVIC；
- 实现 IRQ handler 的最小搬运；
- 保存 DMA/ring 状态和硬件错误计数。

adapter 不可以：

- 解析 CRSF、主从或地面站帧；
- 决定 failsafe；
- 修改 PID；
- 直接写飞行日志文本；
- 隐藏无界等待。

## 9. 迁移清单

| 顺序 | 现有模块 | 目标 HAL | 风险门槛 |
|---:|---|---|---|
| 1 | LED | GPIO | 已完成 |
| 2 | BlueSerial | UART | 保持 DMA buffer 与调参兼容 |
| 3 | SlaveMCU | UART | 保持现有 wire protocol |
| 4 | CRSF | UART | 失联和帧时序回归 |
| 5 | BMP390/QMC | SPI/I2C | 数据与校准对比 |
| 6 | MPU6050/MyI2C | I2C | 500 Hz WCET 与姿态回放 |
| 7 | PWM4 | PWM/TIMER | 示波器确认 PWM 完全一致 |

每次迁移必须保留兼容 wrapper、Host Fake、双固件构建和无桨台架结果。
