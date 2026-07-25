# Flight Data Logger

## 1. 目标

Flight Data Logger 用于记录飞行状态快照，支持后续问题复现、参数分析和地面站
下载，同时不能阻塞控制任务。

当前实现位于 `Shared/Services/flight_data_logger.*`，尚未接入 `main.c`。这保证
新增基础设施不会改变控制周期。

## 2. 记录内容

每条 `FlightDataRecord` 固定 60 bytes，schema version 为 1：

| 字段 | 单位/格式 |
|---|---|
| timestamp | ms，单调时基 |
| roll/pitch/yaw | degree，3 × float32 |
| roll/pitch/yaw rate | degree/s，3 × float32 |
| PID output | 当前算法输出单位，3 × float32 |
| motor output | timer ticks，4 × uint16 |
| battery voltage | mV |
| battery current | mA，int16 |
| system state | 统一安全状态 ID |
| fault mask | Fault Manager active mask |
| flags | 数据有效性位 |
| schema version | uint16 |

第一版 PID output 单位必须在真正接入时固定；在单位批准前不可对外承诺数值
含义。

## 3. 非阻塞路径

```text
Control/Snapshot producer
    |
    | fixed copy, optional decimation
    v
Static ring buffer
    |
    | bounded drain in Logger Task
    v
UART DMA / Flash backend
```

规则：

- storage 由调用方静态提供；
- Capture 不格式化、不分配、不调用 sink；
- `capture_divider` 在生产者入口完成固定分频；
- ring 满时丢弃新记录，保留旧故障上下文；
- dropped count 饱和而不回绕；
- drain 每次最多处理指定数量；
- sink 失败时队首记录不出队；
- 只有一个消费者；
- ISR 不直接 Capture 完整飞行记录。

## 4. 带宽和 RAM 预算

| 采样率 | 原始数据率 |
|---:|---:|
| 500 Hz | 30,000 bytes/s，不推荐 |
| 100 Hz | 6,000 bytes/s |
| 50 Hz | 3,000 bytes/s，初始建议 |
| 20 Hz | 1,200 bytes/s |

示例：

- 32 条 buffer：`32 × 60 = 1920 bytes`；
- 16 条 buffer：`960 bytes`。

STM32F103C8 RAM 有限，不能默认分配 32 条。初始建议 8～16 条，并通过 DMA
及时搬运。最终大小必须和 FreeRTOS stack、协议 buffer、参数 scratch 一起做
静态 RAM 预算。

## 5. 快照一致性

姿态、角速度、PID 和电机输出来自不同执行点时，逐字段读取会产生跨周期混合
记录。接入时必须：

1. 在一个明确任务边界生成 `FlightDataSnapshot`；
2. 使用短临界区、双缓冲或版本计数保证快照一致；
3. 临界区内只复制数值；
4. 电池和低频状态允许使用最近一次已验证值；
5. `flags` 标记缺失、过期或降级数据。

## 6. 与结构化 Event Log 的区别

| Flight Data Logger | Event Log |
|---|---|
| 周期快照 | 离散事件 |
| 固定 60-byte 多字段数据 | 固定 16-byte 单事件 |
| 用于曲线和回放 | 用于故障时间线 |
| 20～100 Hz | 事件发生时 |

两者可以共享最终存储介质，但不能混用记录 schema。

## 7. 接入步骤

1. 定义快照所有字段的权威单位；
2. 建立 50 Hz 后台 capture 点，不在 500 Hz 内环直接输出；
3. 测量 Capture 最坏执行时间；
4. 建立 UART DMA sink Fake 和真实 adapter；
5. 加入 record framing 和文件/Flash 页校验；
6. 注入 sink busy、buffer full 和断电；
7. 地面工具按 schema version 解析；
8. 无桨运行验证 scheduler overrun 不增加。
