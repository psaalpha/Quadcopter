# 架构

两颗 STM32F103C8T6 分别运行 FreeRTOS。主控执行飞行控制，从控执行传感器服务和显示；两端共用 CMSIS/SPL、协议和 DMA 辅助模块。

## 模块职责

| 目录 | 职责 |
|---|---|
| `Master_MCU/Core` | 启动、任务创建、控制编排和中断入口 |
| `Master_MCU/App` | 周期调度与安全状态机 |
| `Master_MCU/BSP` | 时钟、定时器、电机 PWM、LED 和看门狗 |
| `Master_MCU/Control` | 姿态估计、滤波和 PID |
| `Master_MCU/Drivers` | MPU6050、软件 I2C、CRSF、蓝牙和从控链路 |
| `Slave_MCU/Core` | 启动与中断入口 |
| `Slave_MCU/App` | 传感器服务、显示和 RTOS 任务 |
| `Slave_MCU/BSP` | 板级输入、舵机、电池 ADC 和蜂鸣器 |
| `Slave_MCU/Drivers` | 传感器、主控链路、OLED 和 OSD |
| `Shared/Protocol` | 主从编解码和 CRC |
| `Shared/Scheduling` | 周期事件、超期统计与任务健康检查 |
| `Shared/Drivers` | STM32 DMA 接收和毫秒计时辅助实现 |
| `Platform/STM32F1` | CMSIS、SPL 和公共延时 |
| `Platform/FreeRTOS` | 固定版本内核、端口和本项目配置 |

`Shared/Protocol` 和 `Shared/Scheduling` 不访问硬件寄存器；`Shared/Drivers` 可以依赖 STM32 平台。
驱动负责设备和总线，BSP 负责板级资源；公共平台不依赖飞控业务。
Bosch BMP390 原厂文件和 FreeRTOS 内核保留上游内容与许可证。

项目自有标识符采用 `lower_snake_case`，类型使用 `_t`，宏使用 `UPPER_SNAKE_CASE`。
公开接口带模块前缀，物理量使用 `_ms`、`_mm`、`_deg`、`_dps` 等单位后缀。
中断向量及第三方接口保留原约定。

## 任务与周期

| 任务 | 职责 | 触发/周期 | 优先级 | 栈：字/字节 |
|---|---|---|---:|---:|
| `master_control_task` | 安全、遥控、IMU、PID、混控、主从接收 | TIM2 每 1ms 与 RX 通知；内部保留 2/5/10ms 相位 | 5 | 768 / 3072 |
| `master_background_task` | 蓝牙调参和遥测 | 每轮 `vTaskDelay(1)`，遥测快照 100ms 更新 | 2 | 512 / 2048 |
| `service_task` | 从控采集、输入、ADC、主从发送与校准 | TIM2 每 1ms 与 DMA/EXTI；采集/发包 50ms | 4 | 1024 / 4096 |
| `display_task` | OLED、OSD 与显示格式化 | 200ms 快照；每 1ms 至多处理一块显示 | 1 | 640 / 2560 |
| idle | 内核空闲任务 | 无其他 Ready 任务时 | 0 | 128 / 512 |

两块板的任务优先级分别生效，数值越大优先级越高。内核启用抢占，关闭同级时间片。
`periodic_tasks` 负责周期相位、事件合并和超期计数；通知位仅用于唤醒，字节数据仍由 DMA 缓冲保存。
主控采样、姿态、PID 和电机 CCR 更新由同一个控制任务顺序执行。

## 控制与数据路径

```text
CRSF → 遥控映射与安全状态 → 目标姿态
MPU6050 → 姿态估计 → 100Hz 角度外环 → 500Hz 角速度内环 → 混控 → 50Hz PWM
从控传感器 → 41字节协议 → 磁力计航向修正与高度/光流辅助输入
蓝牙输入 → 调参命令队列 → 控制任务
从控传感器快照 → 显示邮箱 → 显示任务
```

磁力计仅融合有效新样本，偏航误差使用 0/360° 最短方向。
辅助模式要求安全状态、油门和传感器输入满足门限；退出或数据超过 200ms 未更新时清除辅助积分。
高度/光流 PID 默认增益为零。光流 X/Y 为原始积分量，方向、尺度和增益仍需标定。

控制任务独占遥控映射、安全状态、姿态估计、PID 和电机 CCR；主从解析也在该任务中执行。
蓝牙通过长度 4 的值队列发送 `tuning_command_t`，控制任务每轮最多取一条且不等待。
队列满时后台暂停消费新命令；输入缓冲继续溢出时执行驱动丢弃与重同步策略。
遥测使用长度 1 的 float 邮箱，从控显示使用长度 1 的结构体邮箱，均以新快照覆盖旧快照。
当前外设由单任务拥有，未启用 mutex 或二值信号量。

## 时基与中断

SysTick 为 FreeRTOS 提供 1kHz 内核时基；TIM2 每 1ms 发布周期事件并使用 DWT 核周期记账。
ISR 完成硬件应答和事件记录，通过任务通知唤醒业务任务；PendSV、SVC、SysTick 由官方 ARM_CM3 端口拥有。
启动时调度器尚未就绪的事件由驱动保留，在任务启动后处理。

| IRQ | SPL 未移位优先级 | 内核 API |
|---|---:|---|
| TIM2 | 6 | 可调用 FromISR |
| 主控 CRSF USART2/DMA6、主从 USART3/DMA3 | 7 | 可调用 FromISR |
| 从控光流 USART1/DMA5、主从 TX DMA7 | 7 | 可调用 FromISR |
| 蓝牙 USART1/DMA4/5、从控 EXTI | 8 | 符合 FromISR 范围；蓝牙保持 pending 轮询 |
| PendSV、SysTick | 15 | 内核拥有 |
| 预留 | 0..4 | 不调用 FreeRTOS API |

两端使用 `NVIC_PriorityGroup_4`，4 位抢占、0 位子优先级。
`configMAX_SYSCALL_INTERRUPT_PRIORITY=0x50` 是左移后的 BASEPRI 值；SPL 设置使用未移位数值。

`Delay_us` 使用 DWT CYCCNT，短 I2C 延时仍为忙等；运行期长等待使用状态机。
Flash 擦写短时屏蔽中断，TIM2 恢复后用 DWT 补记真实经过时间；校准和安全超时不依赖可能丢 tick 的内核时基。
CYCCNT 约 59.6s 回绕，需要持续运行并频繁读取；运行时换时钟和调试暂停不在当前计时支持范围。

## DMA 与 Flash 所有权

CRSF 使用 512B 接收环，420000 baud 满线约 12.2ms 绕圈，每 5ms 最多解析 256B。
HT/TC 记录回绕，IDLE/错误触发通知；溢出时丢弃不确定字节，TE/UART 错误后重新同步。
其他 RX 保持 256B 缓冲与 64B 有界处理批次。IDLE 仅表示字节间隙，组帧由协议状态机完成。

从控 41 字节 TX 缓冲在 DMA 使用期间不可覆盖；busy 时跳过本次发布，完成后由服务任务回收，20ms 异常超时执行回收路径。
校准数据写入从控最后 1KB Flash，保存前协调在途发送和光流服务；相关硬件行为见[待验证项](TESTING.md)。

## 内存与诊断

任务、TCB、队列和 idle 存储均静态分配，`configSUPPORT_DYNAMIC_ALLOCATION=0`，不编译 `heap_x.c`。
启动 MSP 为 1024B，C 库保留 heap 为 512B；两者与 FreeRTOS PSP 任务栈分别预算。
栈溢出检查使用模式 2，assert/故障 hook 进入停机路径。

`master_*_stack_free`、`slave_*_stack_free` 以 32 位字记录剩余栈水位。
`*_max_step_cycles` 记录一次服务循环最大核周期并包含 ISR 抢占；在 72MHz 下除以 72 得到微秒。
这些指标需要上板测量，现有静态预算见[测试与验证](TESTING.md)。

主控运行期不使用 stdio；从控仅显示任务调用 `snprintf`。MicroLIB 的显示与软浮点路径需硬件复核。
故障停机、任务心跳和遥控失联策略集中见[安全状态](SAFETY.md)。
内核版本与许可证见[依赖说明](../Platform/FreeRTOS/README.md)。
