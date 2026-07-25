# 主从传感器协议

本文只描述 Master/Slave 之间的传感器链路。面向调参与状态读取的外部接口见
[地面站通信协议](GROUND_STATION_PROTOCOL.md)，两套协议独立版本化。

## 物理链路

- Slave：USART2 TX，PA2
- Master：USART3 RX，PB11
- 115200 baud，8N1
- 固定帧长 41 字节

## 帧格式（版本 1）

所有多字节整数使用小端序。

| 偏移 | 长度 | 字段 | 单位/说明 |
|---:|---:|---|---|
| 0 | 1 | Magic 0 | `0xA5` |
| 1 | 1 | Magic 1 | `0x5A` |
| 2 | 1 | Version | `1` |
| 3 | 1 | Message type | `1` = sensor data |
| 4 | 1 | Payload length | `32` |
| 5 | 2 | Sequence | 每帧加一，自然回绕 |
| 7 | 2 | Flags | 传感器有效性和告警 |
| 9 | 4 | Timestamp | 从控启动后的毫秒数 |
| 13 | 4 | Pressure | Pa，`int32_t` |
| 17 | 2 | Temperature | 0.01 °C，`int16_t` |
| 19 | 4 | Baro altitude | mm，`int32_t` |
| 23 | 2 | Yaw | 0.01°，`uint16_t` |
| 25 | 4 | Flow X | 原始积分值，`int32_t` |
| 29 | 4 | Flow Y | 原始积分值，`int32_t` |
| 33 | 2 | Flow distance | mm |
| 35 | 1 | Flow quality | 0–100 |
| 36 | 2 | Battery | mV |
| 38 | 1 | Reserved | 当前必须为 0 |
| 39 | 2 | CRC16 | 覆盖字节 0–38 |

CRC 使用 CRC16-CCITT-FALSE：多项式 `0x1021`，初始值 `0xFFFF`，不反射，不额外异或。

## 状态标志

| Bit | 含义 |
|---:|---|
| 0 | 气压计数据有效 |
| 1 | 磁力计数据有效 |
| 2 | 光流数据有效 |
| 3 | 电池低压 |
| 4 | 磁力计正在校准 |
| 5 | 电池测量有效 |

## 兼容策略

- 任何字段或语义破坏性变化都必须增加 `Version`。
- 只增加新的消息类型时保留现有版本，并分配新的 `Message type`。
- 接收端必须先检查 magic、版本、类型、长度和 CRC，再发布数据。
- Master/Slave 必须作为匹配版本一起烧录。
