# 结构化日志系统

## 1. 设计目标

飞控日志必须满足：

- 高频路径写入有确定上界；
- 不动态分配内存；
- 不在控制任务中格式化字符串；
- 输出后端变慢时不阻塞控制；
- 缓冲区溢出可观测；
- 同一记录可在串口、Flash 或主机测试中解释。

`Shared/Services/event_log.*` 提供第一阶段基础设施。它尚未接入现有遥测，
因此不会改变当前串口带宽和任务时序。

## 2. 固定记录格式

每条 `EventLogRecord` 固定为 16 bytes：

| 字段 | 类型 | 含义 |
|---|---|---|
| `timestamp_ms` | `uint32_t` | 单调毫秒时间 |
| `argument` | `int32_t` | 事件相关的一个数值参数 |
| `module_id` | `uint16_t` | 模块编号 |
| `event_id` | `uint16_t` | 模块内事件编号 |
| `level` | `uint8_t` | Debug/Info/Warning/Error/Fatal |
| `flags` | `uint8_t` | 后续扩展，当前写入 0 |
| `reserved` | `uint16_t` | 保留，当前写入 0 |

记录中不保存字符串。地面工具根据固件版本、module ID 和 event ID 映射为
可读文本。

## 3. 环形缓冲

调用方提供静态 `EventLogRecord[]`：

```text
producer -> fixed ring -> low-priority sink
```

策略：

- `EventLog_Write/Push()` 为 O(1)；
- 缓冲满时拒绝新记录；
- 每次拒绝增加 `dropped_count`，计数饱和而不回绕；
- 已存在的故障历史不会被新日志覆盖；
- `Peek()` 不移动 tail；
- sink 成功后才 `Pop()`；
- sink 失败时当前记录仍留在队列中。

## 4. 并发模型

初始化时可以提供成对的 `enter/exit` 回调：

- 当前裸机版本可绑定保存/恢复中断状态的短临界区；
- FreeRTOS 版本可绑定适合当前上下文的临界区；
- 不提供回调表示日志实例只在单一执行上下文访问；
- enter 和 exit 必须同时提供；
- 临界区内只复制固定记录和更新索引，不调用 sink。

第一阶段约束：

- `EventLog_Write()` 只允许在主循环或任务上下文调用；
- ISR 只更新专用计数器或发布诊断事件；
- 禁止 ISR 与任务同时使用一个未提供 ISR-safe 适配器的日志实例；
- 同一个日志实例只有一个消费者。

后续如需 ISR 日志，应单独提供 `EventLog_WriteFromIsr()` 适配器或独立 SPSC
缓冲，不复用可能获得 mutex 的任务 API。

## 5. 输出后端

`EventLogSink` 只有一个非阻塞 `write` 回调。`EventLog_Drain()` 最多处理调用方
指定的记录数，便于限制每轮后台任务耗时。

推荐后端：

| 后端 | 用途 | 约束 |
|---|---|---|
| UART/DMA | 在线调试 | DMA busy 时返回失败，下轮重试 |
| 外部 Flash | 飞行记录 | 分页缓冲，禁止控制任务等待擦写 |
| 主机 Fake | 自动测试 | 捕获记录并注入失败 |
| SWO（其他 MCU） | 开发诊断 | STM32F103C8 当前板卡需确认调试资源 |

当前蓝牙 `[plot,...]` 遥测不是日志 sink，暂时保持原样。

## 6. ID 管理

后续建立唯一注册表：

```text
module 0x0001: system
module 0x0002: flight safety
module 0x0003: RC/CRSF
module 0x0004: inter-MCU link
module 0x0005: parameter service
module 0x0006: scheduler
```

规则：

- module/event ID 发布后不能复用为不同含义；
- 参数单位写在事件注册表中；
- 同一事件只使用一种 argument 语义；
- 需要多个数值的诊断使用多条事件或专用遥测消息；
- Fatal 不代表立即复位，复位策略由 supervisor 决定。

## 7. 建议接入顺序

1. 启动和复位原因；
2. 飞行安全状态切换及原因；
3. CRSF 失联和恢复；
4. 主从协议 CRC、版本和超时计数；
5. scheduler overrun；
6. 参数加载、损坏恢复和保存失败；
7. 传感器健康状态。

每次只接入一个模块，并测量最终 Code、RAM、任务耗时和 dropped count。

## 8. 发布验收

- 日志缓冲大小有 RAM 预算；
- 满队列行为经过测试；
- sink 失败不会丢掉队首；
- 控制路径没有 `printf/sprintf`；
- ISR 没有阻塞日志调用；
- 地面解析表与固件版本匹配；
- 无日志消费者时，飞行安全行为保持不变。
