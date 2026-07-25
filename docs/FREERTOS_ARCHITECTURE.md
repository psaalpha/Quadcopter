# FreeRTOS 任务架构设计

## 1. 当前状态

当前固件仍使用经过验证的协作式调度器，FreeRTOS 内核没有加入工程，也没有
默认启用。本阶段只建立可测试的任务清单和迁移约束，避免“先运行起来、再补
时序和资源设计”。

执行模型由 `APP_EXECUTION_MODEL` 保护，目前唯一允许值是
`APP_EXECUTION_MODEL_COOPERATIVE`。在真正实现适配器前选择 FreeRTOS 会
在编译期失败。

## 2. 为什么不直接加入 FreeRTOS

直接替换当前主循环会同时改变：

- 周期来源：硬件定时器通知变为 RTOS tick 或软件定时器；
- 调度顺序：固定主循环顺序变为抢占优先级；
- 共享数据：原短临界区可能变成任务与 ISR 并发；
- 看门狗：喂狗位置可能掩盖单任务卡死；
- RAM：每个任务增加独立栈和内核对象；
- 抖动：高优先级任务和临界区会影响 500 Hz 内环；
- 故障行为：栈溢出、队列满和优先级反转成为新故障模式。

因此必须先固定任务模型、测量当前最坏执行时间，再引入内核。

## 3. 任务清单

权威定义在 `Master_MCU/App/app_task_model.*`。周期继续引用
`BSP/board_config.h`，避免文档、定时器和 RTOS 配置出现三份数值。

| 任务 | 周期/Deadline | 逻辑优先级 | 初始栈预算 | 发布策略 |
|---|---:|---:|---:|---|
| IMU 与内环 | 2 ms | Critical | 256 words | 合并重复发布 |
| CRSF 服务 | 5 ms | High | 192 words | 合并重复发布 |
| 角度外环 | 10 ms | Normal | 192 words | 合并重复发布 |
| 电机输出 | 20 ms | High | 192 words | 合并重复发布 |

四个任务初始栈预算合计 `832 words = 3328 bytes`。这只是设计上限，不是
已经验证的最终值；启用 RTOS 后必须用 stack high-water mark 实测并留出
余量。

## 4. 推荐 FreeRTOS 映射

### 4.1 周期任务

第一版适配器建议每个任务使用静态任务对象和 `vTaskDelayUntil()`：

```text
Task loop
  -> wait until absolute release time
  -> execute one bounded application step
  -> record execution time / deadline miss
```

禁止使用相对 `vTaskDelay()` 作为控制周期，因为执行时间会累积到周期中产生
漂移。

当前定时器 ISR 的任务发布逻辑需要在 RTOS 版本中移除或改为测量时基，不能
同时保留硬件定时器发布和 `vTaskDelayUntil()`，否则同一任务会出现双时钟源。

### 4.2 ISR 通知

UART/DMA/EXTI 事件采用直接任务通知或静态队列：

- ISR 只搬运最小数据并调用 `...FromISR` API；
- 使用 `portYIELD_FROM_ISR()` 处理必要的立即切换；
- ISR 不解析协议、不进行浮点运算、不输出日志；
- 队列满必须计数，不能静默覆盖安全相关事件。

### 4.3 静态内存

飞控版本要求：

- `configSUPPORT_STATIC_ALLOCATION = 1`；
- 控制任务、Idle、Timer（如启用）均提供静态 TCB 和栈；
- 默认禁止应用层 `pvPortMalloc()`；
- 队列、事件组和互斥量使用静态创建 API；
- 链接 map 中记录 RTOS RAM 增量。

## 5. 优先级与共享资源

建议优先级顺序：

```text
IMU/inner loop
    >
RC service == motor output
    >
angle outer loop
    >
parameter persistence / telemetry / log drain
```

约束：

- 控制任务不得等待日志、参数保存或遥测发送；
- I2C/SPI 互斥必须启用优先级继承；
- 不允许在持有总线互斥量时等待另一个队列；
- 多字段传感器数据使用快照、双缓冲或队列，不逐字段公开；
- 参数更新采用低优先级验证和一次性提交；
- Flash 擦写只允许在 DISARM 且控制任务明确许可时发生。

## 6. 看门狗设计

不能由单一空闲任务无条件喂狗。推荐：

1. 每个关键任务在完成一次有效迭代后更新 heartbeat；
2. 独立 supervisor 检查 heartbeat、deadline miss 和栈余量；
3. 所有关键任务健康且飞行安全状态有效时才喂硬件看门狗；
4. 重启前尽可能保存复位原因和最后故障事件；
5. 调试版本可暂停看门狗，但发布版本不得关闭。

## 7. 运行时观测

每个任务至少记录：

- 最近一次开始和结束时间；
- 最大执行时间；
- deadline miss 次数；
- 发布合并或通知丢失次数；
- stack high-water mark；
- 最近一次 heartbeat。

这些数据应进入结构化诊断接口，不在高频任务中格式化字符串。

## 8. 迁移步骤

1. 当前阶段：建立并测试任务清单；
2. 抽取四个任务的单次执行函数，确保不包含无限等待；
3. 加入 FreeRTOS 源码、配置和静态内存预算，但默认不启用；
4. 实现 `freertos` 执行适配器；
5. 在无桨台架比较协作式与 RTOS 的周期、输出和失联行为；
6. 完成栈溢出、队列满、任务卡死和优先级反转故障注入；
7. 单独提交默认执行模型切换；
8. 保留至少一个发布周期的协作式回退配置。

## 9. 启用验收

默认启用 FreeRTOS 前必须同时满足：

- Master/Slave 零错误零警告；
- 所有主机测试通过；
- 静态 RAM 预算和 map 审核通过；
- 500 Hz 任务最坏执行时间和抖动满足预算；
- 24 小时无桨运行无栈增长、死锁和看门狗复位；
- CRSF 失联、主从数据异常和任务卡死均能进入安全输出；
- 同一输入回放下，控制输出与协作式基线的差异在批准范围内。
