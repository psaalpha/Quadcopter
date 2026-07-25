# 地面站通信协议

本文档定义 STM32 飞控端的稳定接口。它与主从 MCU 传感器协议相互独立，
不修改现有主从协议、PID、姿态解算、电机混控或 PWM 时序。本阶段只提供协议编解码
与命令分发边界，不实现 Linux 地面站，也不把协议接入实际 UART。

## 分层与职责

```text
UART RX/TX backend（后续接入）
        ↓
GroundStationProtocol：帧、长度、字节序、CRC
        ↓
GroundStationService：请求校验与业务回调分发
        ↓
状态快照 / ParameterStore / Safety command adapter
```

- Protocol 不知道参数、飞行状态和电机；
- Service 不直接读写寄存器或执行解锁；
- 应用适配器负责权限、安全状态和实际业务；
- 所有控制请求都必须经过应用层安全条件判断。

## 帧格式（Version 1）

所有多字节字段使用小端序。

| 偏移 | 长度 | 字段 | 说明 |
|---:|---:|---|---|
| 0 | 1 | Magic 0 | `0x51` (`Q`) |
| 1 | 1 | Magic 1 | `0x47` (`G`) |
| 2 | 1 | Version | 当前为 `1` |
| 3 | 1 | Message type | 请求、响应或错误类型 |
| 4 | 1 | Flags | 当前保留，发送方写 0 |
| 5 | 1 | Reserved | 必须写 0 |
| 6 | 2 | Sequence | 请求响应关联，自然回绕 |
| 8 | 2 | Payload length | `0..48` |
| 10 | N | Payload | 按消息定义 |
| 10+N | 2 | CRC16 | 覆盖 header 和 payload |

最短帧 12 字节，最大帧 60 字节。CRC 使用
CRC16-CCITT-FALSE：多项式 `0x1021`、初值 `0xFFFF`、无反射、无最终异或。

## 消息类型

| 请求 | 值 | 成功响应 | 值 |
|---|---:|---|---:|
| Get status | `0x01` | Status | `0x81` |
| Parameter get | `0x02` | Parameter value | `0x82` |
| Parameter set | `0x03` | Parameter accepted | `0x83` |
| Parameter list | `0x04` | Parameter metadata | `0x84` |
| Control | `0x05` | Control accepted | `0x85` |
| — | — | Error | `0xFF` |

响应复制请求的 sequence。错误响应 payload 为：

| 偏移 | 长度 | 字段 |
|---:|---:|---|
| 0 | 1 | 原请求 message type |
| 1 | 1 | `GroundStationServiceStatus` |

## Payload 定义

### 状态读取

请求 payload 长度为 0。响应固定 18 字节：

| 偏移 | 长度 | 字段 | 单位 |
|---:|---:|---|---|
| 0 | 4 | uptime | ms |
| 4 | 4 | active fault mask | bit mask |
| 8 | 2 | battery | mV |
| 10 | 2 | roll | 0.01°，有符号 |
| 12 | 2 | pitch | 0.01°，有符号 |
| 14 | 2 | yaw | 0.01°，有符号 |
| 16 | 1 | system state | 应用定义枚举 |
| 17 | 1 | armed | 0/1 |

应用回调必须从一致快照生成状态，不能在序列化期间读取一组可能被中断逐项更新的数据。

### 参数读取

请求：`id:u16`。响应：`id:u16, type:u8, reserved:u8, raw_value:u32`。
`raw_value` 是按 type 解释的原始 32 位值，不在协议层进行浮点转换。

### 参数修改

请求：`id:u16, type:u8, reserved:u8, raw_value:u32`。
成功响应：`id:u16, type:u8, reserved:u8`。

修改必须由参数系统完成类型、范围、权限和 schema 校验。协议成功只表示当前请求被接受，
不代表已经写入 Flash；持久化使用独立控制命令。

### 参数列表

请求：`index:u16`。响应：
`index:u16, id:u16, type:u8, category:u8, flags:u16, name_length:u8, name:bytes`。
名称最大 31 字节，不包含结尾 `\0`。越界 index 返回 `NOT_FOUND`。

### 控制命令

请求固定 8 字节：`command:u8, reserved[3], argument:i32`。成功响应为
`command:u8, reserved[3]`。

| Command | 值 | 语义 |
|---|---:|---|
| DISARM | 1 | 请求立即进入安全停机 |
| REQUEST_ARM | 2 | 请求解锁，必须经过应用安全守卫 |
| CLEAR_FAULTS | 3 | 请求清除允许清除的故障 |
| SAVE_PARAMETERS | 4 | 请求异步保存参数 |
| RESET | 5 | 请求受控复位 |

未知命令在调用应用回调前被拒绝。特别地：

- DISARM 可以设计为幂等操作；
- REQUEST_ARM 不能绕过油门、姿态、传感器、通信和故障检查；
- RESET、清故障和保存参数应根据 armed 状态限制；
- 后续对远程链路开放时应增加会话认证、重放防护和命令授权；
- 接收超时或 CRC 错误不能改变飞行控制输出。

## 流式 UART 接入约束

当前 codec 接收完整帧。后续 UART 适配器应使用 DMA/中断环形缓冲，在通信任务中完成：

1. 搜索 magic；
2. 等待完整 header；
3. 校验 payload length；
4. 等待完整帧并校验 CRC；
5. 调用 Service；
6. 把响应放入有界 TX 队列。

ISR 只搬运字节并发布事件，不执行参数写入、控制命令、CRC 全帧扫描或阻塞发送。

## 兼容策略

- 破坏帧格式、字段含义或安全语义时提升 Version；
- 新增消息优先分配新 message type；
- 现有消息只允许在明确版本下追加可选字段；
- 未识别版本、类型、错误长度和错误 CRC 必须明确拒绝；
- 主从 MCU 协议继续由 [PROTOCOL.md](PROTOCOL.md) 单独管理。

## 当前实现边界

- 已实现：纯 C 编解码、CRC、请求分发、回调接口和主机测试；
- 未实现：UART 流解析器、认证、命令限流、Linux 地面站；
- 未接入：当前控制循环、现有串口中断和飞行状态；
- 接入前必须增加硬件在环测试、模糊测试与安全用例。
