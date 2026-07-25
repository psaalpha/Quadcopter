# 系统可靠性设计

## 1. 当前接入边界

本阶段增加三个硬件无关模块：

- `WatchdogManager`：只有关键客户端全部健康时才允许喂硬件狗；
- `FaultManager`：统一记录故障 ID、等级、时间、参数和历史；
- `FailsafeStateMachine`：统一处理 `NORMAL/WARNING/FAILSAFE/DISARM`。

这些模块暂未替换现有 `FlightSafety`、`IWDG_ReloadCounter()` 或电机输出路径。
原因是可靠性基础设施和飞行控制接入必须分两个提交完成：先验证规则，再进行
无桨台架对比。

## 2. Watchdog Manager

### 2.1 解决的问题

当前主循环无条件喂狗时，只能发现整个 CPU 卡死，不能发现“主循环仍运行，
但 IMU、通信或控制任务已经停止”的局部失效。

Watchdog Manager 将喂狗权收敛到一个监督器：

```text
Sensor heartbeat ----\
Control heartbeat ----+--> health check --> hardware feed callback
Safety heartbeat -----/
```

### 2.2 接口

1. `WatchdogManager_Init()`：
   - 读取并保存 RCC/平台复位原因；
   - 清除平台复位标志；
   - 初始化硬件看门狗后端；
2. `WatchdogManager_RegisterClient()`：
   - 注册 client ID、最大 heartbeat 间隔和是否为 required；
3. `WatchdogManager_Heartbeat()`：
   - 由完成一次有效工作的模块报告；
4. `WatchdogManager_Check()`：
   - 使用无符号时间差检查缺失与超时；
5. `WatchdogManager_Feed()`：
   - 只有所有 required client 已报告且未超时才调用硬件 feed；
6. `WatchdogManager_GetResetRecord()`：
   - 返回启动阶段捕获的复位原因。

### 2.3 关键规则

- heartbeat 表示“有效工作完成”，不能只表示任务被调度；
- required client 从未报告时禁止喂狗；
- 超时后禁止喂狗，让硬件看门狗完成复位；
- feed 被拒绝和实际 feed 分别计数；
- client 超时使用 `(uint32_t)(now - last)`，支持 tick 回绕；
- 后端负责把 STM32 RCC reset flags 映射为统一 reason mask；
- 发布版本不能通过错误处理路径无条件绕过 manager 喂狗。

## 3. Fault Manager

### 3.1 Fault ID

当前稳定 ID：

| Fault ID | 含义 | 典型来源 |
|---|---|---|
| `FAULT_ID_IMU` | IMU 初始化、读取或数据健康错误 | Sensor Task |
| `FAULT_ID_COMMUNICATION_TIMEOUT` | CRSF、主从或地面站超时 | Communication Task |
| `FAULT_ID_SENSOR` | 气压计、光流、磁力计等异常 | Sensor Task |
| `FAULT_ID_BATTERY` | 欠压、过流或采样不可信 | Safety Task |
| `FAULT_ID_PARAMETER` | schema、CRC、范围或保存错误 | Parameter Service |
| `FAULT_ID_WATCHDOG` | client heartbeat 超时 | Safety Task |

ID 发布后不能复用为其他含义。未来新增项只能追加。

### 3.2 Fault Level

| Level | 含义 | 建议安全动作 |
|---|---|---|
| `NONE` | 无活动故障 | 无 |
| `INFO` | 诊断事件，不降低功能 | 记录 |
| `WARNING` | 能继续运行，但需要降级或提示 | 进入 WARNING |
| `CRITICAL` | 继续飞行存在直接风险 | 进入 FAILSAFE |
| `FATAL` | 系统完整性无法保证 | 立即进入安全输出并 DISARM |

### 3.3 Fault Record

每个 ID 固定一条记录：

- 当前 level；
- 是否 active；
- 是否 latched；
- 首次和最近时间；
- occurrence count；
- 一个 `int32_t` 诊断参数。

`active_mask` 表示当前故障，`latched_mask` 表示本次运行历史。清除活动故障不会
自动清除 latched 历史；只有明确的维护动作才能 `ResetHistory()`。

### 3.4 Fault Handler

handler 只接收：

- `RAISED`；
- `UPDATED`；
- `CLEARED`。

handler 适合发布结构化日志或通知 Safety Task，不应直接在回调中阻塞、写
Flash 或执行复杂控制。重复 Raise 会增加 occurrence count；活动故障的等级
只升级，不因一次较低等级报告而自动降级。

## 4. Failsafe 状态机

### 4.1 状态定义

| 状态 | 含义 | `MotorsMayRun` |
|---|---|---|
| `DISARM` | 启动、人工停机或 Failsafe 完成后的安全状态 | 否 |
| `NORMAL` | 保护条件满足，正常运行 | 是 |
| `WARNING` | 有可控异常，允许受监督运行 | 是 |
| `FAILSAFE` | 关键故障处理阶段 | 否（当前基础策略） |

状态机上电固定进入 `DISARM`，永不自动解锁。

### 4.2 完整状态转换表

| 当前状态 | 事件 | Guard | 下一状态 | 说明 |
|---|---|---|---|---|
| DISARM | ARM_REQUEST | `arm_permitted=true` | NORMAL | 所有解锁条件由上层计算 |
| DISARM | ARM_REQUEST | `arm_permitted=false` | DISARM | 返回 Guard Rejected |
| NORMAL | WARNING_PRESENT | - | WARNING | 可控降级 |
| WARNING | WARNINGS_CLEARED | - | NORMAL | 仅清除 Warning |
| NORMAL/WARNING | CRITICAL_FAULT | - | FAILSAFE | 立即撤销电机许可 |
| FAILSAFE | ACTION_COMPLETE | - | DISARM | Failsafe 后必须重新人工解锁 |
| 任意状态 | DISARM_REQUEST | - | DISARM | 人工停机优先 |
| 任意状态 | RESET | - | DISARM | 软件状态复位 |
| DISARM | CRITICAL_FAULT | - | DISARM | 已安全，不产生自动动作 |
| FAILSAFE | WARNINGS_CLEARED | - | FAILSAFE | 禁止自动恢复飞行 |
| 其他未列事件 | - | - | 原状态 | 无状态变化 |

### 4.3 状态图

```text
                        WARNING_PRESENT
               +----------------------------+
               |                            v
DISARM --ARM--> NORMAL <---WARNINGS_CLEARED-- WARNING
  ^               |                            |
  |               +------CRITICAL_FAULT-------+
  |                            |
  |                            v
  +----ACTION_COMPLETE------ FAILSAFE

DISARM_REQUEST / RESET: any state -> DISARM
```

## 5. 与现有 FlightSafety 的关系

现有 `FlightSafety` 已保护：

- 启动低油门；
- CRSF 失联；
- 恢复低油门。

新状态机是未来系统级监督模型，不能立即替换现有保护。建议接入顺序：

1. 先镜像运行，只比较状态而不控制电机；
2. 将 CRSF timeout 映射为 Fault；
3. 将新旧状态和电机许可做无桨一致性测试；
4. 验证 IMU、主从、电池和参数故障注入；
5. 单独评审后再让新状态机成为唯一电机许可来源；
6. 最后移除重复的旧状态。

## 6. 台架验收

- 未收到所有 required heartbeat 时硬件 feed 不发生；
- 任一 required client 超时后 feed 被持续拒绝；
- reset reason 上电只捕获一次；
- Warning 清除可以恢复 Normal；
- Critical 故障不能自动恢复 Normal；
- Failsafe 完成只能进入 Disarm；
- tick 回绕不产生误超时；
- 任何新模块失效都不能绕过现有低油门与失联保护。
