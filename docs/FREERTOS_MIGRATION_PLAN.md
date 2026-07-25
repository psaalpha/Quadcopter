# FreeRTOS 迁移计划

## 1. 决策与边界

本阶段不引入 FreeRTOS 内核、不改变当前协作式调度器，也不修改 PID、姿态解算、
电机混控和 PWM 时序。目标是先固定未来任务职责、周期、优先级、数据流和资源所有权，
让正式迁移可以逐项测量、对照和回退。

当前执行步骤由 `app_task_model.*` 描述；未来五任务蓝图由
`freertos_task_plan.*` 描述。后者能在 ARMCC 和主机环境编译，但没有任务入口、
TCB、队列或 FreeRTOS 头文件，因此不会改变固件运行行为。

## 2. 目标任务模型

优先级数字越大越高。周期和预算是初始设计值，正式启用前必须由 GPIO trace、
DWT cycle counter 和 stack high-water mark 实测确认。

| Task | 周期 | Deadline | 优先级 | WCET 初始预算 | 栈预算 | 释放方式 |
|---|---:|---:|---:|---:|---:|---|
| Sensor Task | 2 ms | 1 ms | 5 | 500 µs | 256 words | 绝对周期 |
| Control Task | 2 ms | 2 ms | 4 | 500 µs | 320 words | Sensor 完成通知 |
| Communication Task | 5 ms | 5 ms | 3 | 600 µs | 256 words | 周期 + UART 事件 |
| Safety Task | 5 ms | 2 ms | 6 | 250 µs | 256 words | 绝对周期 |
| Logger Task | 20 ms | 20 ms | 1 | 800 µs | 256 words | 周期 + 队列事件 |

静态任务栈初始合计 1344 words，即 5376 bytes；这不包含 TCB、队列、DMA buffer、
FreeRTOS heap/静态对象和 ISR 栈。STM32F103C8 的最终 RAM map 必须单独审核。

### 2.1 Sensor Task

- 唯一拥有 IMU 所需 I2C/SPI transaction；
- 每 2 ms 使用 `vTaskDelayUntil()` 的等价绝对周期释放；
- 完成采样、校验和预处理后发布不可变传感器快照；
- 最后通知 Control Task；
- 总线错误上报 Fault Manager，不在任务内无限重试。

### 2.2 Control Task

- 等待 Sensor Task 的直接任务通知，而不是独立软件延时；
- 读取一份完整传感器快照和最近有效 RC command；
- 调用原有姿态解算、PID 和 mixer 单步函数；
- 作为电机 command 的唯一生产者；
- 不等待串口、日志、参数保存或 Flash；
- PWM 写入仍通过既有确定时序路径，迁移首版不改变更新点。

### 2.3 Communication Task

- 拥有 UART RX parser 和 TX queue；
- 处理 CRSF、主从协议和地面站协议的边界适配；
- 将 RC command、参数请求和控制请求发布给对应所有者；
- 不直接修改正在使用的 PID 参数实例；
- 每轮发送有字节预算，队列满和 CRC 错误计数。

### 2.4 Safety Task

- 最高优先级，但必须保持短且有界；
- 检查 Watchdog heartbeat、Fault、通信超时、电池和传感器健康；
- 驱动 NORMAL/WARNING/FAILSAFE/DISARM 状态机；
- 发布只读 safety decision，Control/PWM 路径必须服从；
- 只有所有 required heartbeat 健康时才允许喂硬件看门狗。

### 2.5 Logger Task

- 最低优先级；
- 从有界 ring buffer 批量取出 flight records 和 events；
- 每轮限制最大记录数或最大字节数；
- 后端忙时让出 CPU，不反压控制任务；
- 丢弃策略和 dropped counter 必须可观测。

## 3. 数据流

```text
IMU ISR/DMA
    → Sensor Task
    → SensorSnapshot (double buffer + generation)
    → Control Task
    → MotorCommand
    → PWM owner

UART ISR/DMA
    → Communication Task
    → RcCommandSnapshot / ParameterRequestQueue / ControlRequestQueue

Sensor heartbeat + Control heartbeat + comm timeout + battery
    → Safety Task
    → SafetyDecision
    → Control/PWM gate + Watchdog Manager

SensorSnapshot + ControlSnapshot + MotorSnapshot + FaultSnapshot
    → FlightDataLogger ring
    → Logger Task
    → bounded backend
```

规则：

- 跨任务传递多字段数据必须以快照、双缓冲或队列为单位；
- ISR 只搬运、标记时间和通知任务；
- 单一 writer，多个 reader；不得公开“写一半”的结构；
- 参数修改先在通信上下文校验，再在受控提交点原子切换；
- 日志只消费副本，永远不能持有控制数据锁。

## 4. 资源竞争与所有权

| 资源 | Owner | 其他访问者 | 机制 |
|---|---|---|---|
| IMU I2C/SPI | Sensor | 诊断命令 | 请求队列；飞行中禁止抢占 |
| UART DMA | Communication | ISR | 静态 RX/TX ring + task notification |
| PWM peripheral | PWM/Control adapter | Safety | 单 writer；Safety 发布 override |
| ParameterStore | Communication/parameter service | Control | immutable active snapshot + atomic commit |
| Flash | persistence worker（后续） | 无 | 仅 DISARM，异步请求，双槽原子提交 |
| Fault Manager | Safety 为策略 owner | 各任务上报 | 短临界区或单消费者事件队列 |
| Event/flight log | Logger 为消费者 | 多生产者 | 有界 ring；短临界区；drop-new |

禁止：

- 持有 I2C/SPI mutex 时等待另一个队列；
- Control Task 获取 Logger 或 UART mutex；
- 在 ISR 中获取普通 mutex；
- 在高优先级任务中擦写 Flash、格式化长字符串或阻塞 TX；
- 用 SuspendAll 长时间保护外设事务。

若必须使用 mutex，总线 mutex 启用优先级继承。更推荐通过资源 owner task +
request queue 消除共享总线锁。

## 5. 时间和调度设计

- `configTICK_RATE_HZ` 初始建议 1000 Hz，但 2 ms 控制链由绝对周期/通知驱动；
- Sensor 完成后通知 Control，减少采样到控制的相位不确定性；
- Safety 优先级最高，只做判定与发布，不执行慢速恢复；
- Logger 最低，持续 backlog 不得提升优先级；
- 开启 runtime stats，记录 release jitter、execution time、deadline miss；
- 关键临界区用 cycle counter 度量，并设代码审查上限；
- tickless idle 在飞行版本完成抖动验证前不启用。

### 初始 2 ms 窗口预算

Sensor 500 µs + Control 500 µs，保留约 1000 µs 给中断、Safety 抢占和抖动。
这只是进入台架测量的门槛，不是实测结论。

## 6. 静态内存和错误钩子

正式迁移要求：

- `configSUPPORT_STATIC_ALLOCATION = 1`；
- 应用任务、队列、mutex、event group 使用静态创建；
- 默认禁止业务层 `pvPortMalloc()`；
- 实现 malloc failed、stack overflow、assert、hard fault 钩子；
- Idle hook 不作为无条件喂狗点；
- Timer Task 非必要不启用；若启用，列入 RAM 和优先级审计；
- 发布构建保留 task stack high-water mark 和 reset reason。

## 7. 分步迁移顺序

1. **测量基线**：记录当前 500 Hz 周期、抖动、执行时间、RAM 和失联响应；
2. **抽取单步入口**：确保 Sensor/Control/Communication/Safety/Logger 都能一次调用后返回；
3. **引入但不启用内核**：加入固定版本 FreeRTOS、配置、静态对象和构建开关；
4. **先迁慢任务**：Logger、Communication 使用 RTOS，控制链保持原调度；
5. **迁 Safety**：验证 heartbeat、任务卡死和 DISARM；
6. **迁 Sensor/Control**：保持原算法调用顺序、输入快照和 PWM 更新点；
7. **双模式对照**：同一录制输入对比姿态、PID 和 motor output；
8. **故障注入**：队列满、总线超时、栈溢出、优先级反转和任务卡死；
9. **无桨耐久**：至少 24 小时运行；
10. **单独切换默认模式**：保留一个发布周期的 cooperative 回退开关。

## 8. 每阶段验收

| 阶段 | 必须证明 |
|---|---|
| 基线 | 周期、WCET、RAM、故障响应有可复现实测记录 |
| 慢任务迁移 | 控制周期无统计显著变化，日志/通信背压有计数 |
| Safety 迁移 | 任一 required task 卡死都阻止喂狗并进入批准的安全行为 |
| 控制链迁移 | 算法调用和 PWM 更新点等价，500 Hz deadline 无丢失 |
| 发布候选 | 双固件 0/0、全测试、24h、HIL、故障注入、回退验证 |

## 9. 明确不在本阶段完成

- 不加入 FreeRTOS 源码；
- 不创建真实任务、队列和 mutex；
- 不修改当前中断优先级分组；
- 不改变现有控制算法和外设时序；
- 不宣称栈和 WCET 预算已经过硬件验证。

已有的概念说明见 [FreeRTOS 任务架构设计](FREERTOS_ARCHITECTURE.md)。
