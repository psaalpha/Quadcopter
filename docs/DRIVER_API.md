# 驱动接口规范

## 目的

本文定义新驱动和后续整理现有驱动时应遵守的接口规则。当前历史驱动尚未全部迁移，规范采用“新增代码立即遵守、现有代码按模块逐步迁移”的方式，避免一次性修改大量稳定代码。

## 分层边界

```text
App/User
   ↓ 调用公开 API
Device Driver
   ↓ 调用总线或 BSP
Bus/BSP
   ↓
STM32 registers / SPL
```

- 应用层不直接操作设备寄存器；
- 设备驱动不决定飞行安全状态；
- 总线驱动不解释传感器业务含义；
- ISR 只处理接收、状态和必要搬运，不执行复杂业务。

## 命名规则

公开符号使用统一模块前缀：

```c
Module_Init()
Module_Deinit()
Module_Start()
Module_Stop()
Module_Process()
Module_Read()
Module_Write()
Module_GetStatus()
Module_IrqHandler()
```

要求：

- 类型使用 `ModuleName` 或 `ModuleNameStatus`；
- 宏使用 `MODULE_NAME_CONSTANT`；
- 私有函数和变量必须是 `static`；
- 头文件不得声明只在 `.c` 内使用的 `static` 函数；
- 不新增无模块前缀的 `GetData()`、`IsValid()` 等全局名称。

## 推荐返回值

新驱动不应只使用 `void` 表示初始化成功。推荐统一状态语义：

```c
typedef enum
{
    DRIVER_STATUS_OK = 0,
    DRIVER_STATUS_INVALID_ARGUMENT,
    DRIVER_STATUS_NOT_READY,
    DRIVER_STATUS_TIMEOUT,
    DRIVER_STATUS_IO_ERROR,
    DRIVER_STATUS_CRC_ERROR
} DriverStatus;
```

状态类型可以由模块自定义，但必须满足：

- `0` 表示成功；
- 错误原因可区分；
- 不使用无文档的魔法数字；
- 调用方能够决定重试、降级或进入故障状态。

## 初始化接口

推荐：

```c
DriverStatus Sensor_Init(const SensorConfig *config);
```

配置应明确：

- 使用的总线实例；
- 地址或片选；
- 超时；
- 采样率；
- 回调或底层读写函数。

如果资源完全由 BSP 固定，可以使用 `Sensor_Init(void)`，但引脚和外设必须在 `PINOUT.md` 和 BSP 中有唯一来源。

## 数据读取接口

推荐使用调用方提供的快照结构：

```c
typedef struct
{
    int32_t value;
    uint32_t timestamp_ms;
    uint16_t status_flags;
} SensorSample;

DriverStatus Sensor_Read(SensorSample *sample);
```

避免：

- 返回大型结构体造成隐式复制；
- 无保护地暴露 ISR 正在写入的全局结构；
- 通过裸 `float *` 或未对齐指针解释通信数据；
- 字段名称不包含单位且文档也未说明。

单位应通过名称或接口文档固定，例如：

- `_ms`、`_us`；
- `_mv`；
- `_pa`；
- `_mm`；
- `_centi_c`；
- `_centi_deg`。

## 非阻塞原则

高频任务调用的驱动必须说明是否阻塞。

- ISR API：禁止等待；
- 500 Hz/200 Hz 任务：避免轮询等待外设；
- 允许阻塞的初始化函数必须有超时；
- DMA 发送必须提供 busy/complete 状态；
- `while(flag == RESET)` 必须评估最坏等待时间。

## ISR 与主循环接口

推荐模式：

1. ISR 接收字节或完成 DMA；
2. ISR 更新最小状态并置位事件；
3. 主循环调用 `Module_Process()`；
4. 主循环获取完整、已验证的数据快照。

共享数据要求：

- ISR 共享标志使用 `volatile`；
- 多字段快照需要短临界区或双缓冲；
- 临界区内不执行浮点、格式化或阻塞操作；
- 计数器自然回绕时使用无符号差值。

## 头文件规范

每个公开头文件应包含：

1. include guard；
2. 自己所需的最小依赖；
3. 公开类型；
4. 公开常量；
5. 公开 API；
6. 单位、线程/中断上下文和返回值说明。

头文件不得依赖“某个其他头文件刚好先被包含”。

输入缓冲区不修改时使用 `const`：

```c
DriverStatus Uart_Send(const uint8_t *data, uint16_t length);
```

## 当前驱动接口现状

| 模块 | 当前优点 | 后续规范化重点 |
|---|---|---|
| `CRSF` | 已有 CRC、帧计数和 Process 边界 | 将公开全局通道逐步改为快照 API |
| `slave_link` | 有协议校验和诊断计数 | 增加数据新鲜度查询，减少公开可写全局 |
| `bluetooth_serial` | DMA 接收通知，主循环组帧/解析；DMA 遥测忙时跳过；重复接口已清理 | 保留必要的溢出和超长帧计数 |
| `MPU6050` | 模块前缀明确 | 初始化/读取增加状态和超时 |
| `motor_pwm` | 输出范围有限制 | 改为电机语义 API，而不是裸 CH1–CH4 |
| `OpticalFlow` | 有有效性和超时处理 | 给 `optical_flow_is_valid/optical_flow_get_x` 增加模块前缀 |
| `QMC5883P` | 初始化和校准阶段明确 | 统一校准返回值，隐藏内部全局状态 |
| `BMP390` | 采用 Bosch 回调式底层接口 | 明确 wrapper 与 vendor 源码边界 |
| `OLED/OSD` | 功能接口直观 | 输入字符串使用 `const`，补充边界约束 |

## 驱动迁移顺序

建议按风险由低到高进行：

1. 仅整理头文件依赖、`const` 和注释；
2. 为现有 API 增加状态查询，不删除旧接口；
3. 增加快照式读取 API，并迁移调用方；
4. 验证后移除公开全局变量；
5. 最后处理命名和目录迁移。

每次只迁移一个驱动，并完成 Master/Slave 构建。涉及传感器数值或控制输入的迁移必须进行无桨对比测试。

## 当前主控串口执行边界

- CRSF 保持原实现：USART2 DMA 环形接收，5ms 主循环解析。
- 蓝牙 USART1：256 字节 DMA 环形接收；IDLE/HT/TC 仅清标志、通知和累计绕圈。主循环 `bluetooth_serial_process()` 每轮最多取 64 字节，收到一条命令即返回，紧接着 `bluetooth_serial_parse_parameters()` 应用；后续命令留待下一轮。
- 从控 USART3：256 字节 DMA 环形接收，115200/8N1，41 字节/帧、20Hz；`slave_link_process()` 每轮最多处理 64 字节。保留半帧、验证 CRC16/版本、坏帧重新同步，控制使用最新有效帧。
- 两条链路共用 `dma_rx`：主循环先取走事件再处理数据，不停止或重置接收 DMA；未读数据达到缓冲容量时保守丢弃并重置协议半帧。HT/TC 通知覆盖没有 IDLE 的连续流；短临界区内只处理游标，恢复原 PRIMASK。若全局关中断超过一整圈，单个硬件 TC 标志无法还原多圈，不能保证恢复全部数据。
- 诊断：蓝牙 `bluetooth_serial_get_rx_overruns()` / `bluetooth_serial_get_rx_frame_errors()`；从控 `slave_sensor_data.rx_overruns` 和已有 CRC/格式/序号缺口计数。没有周期打印。
- PWM：500Hz 内环计算后写四路 CCR，UDIS 短暂阻止半套值被装载；50Hz 更新事件加载预装载值。若更新边界恰落在写入期间，该次加载可能推迟到下一周期。安全路径直接写最小有效比较值并清除旧预装载，不人为重启 PWM 周期。

## 从机 DMA 与事件接口（2026-10-04）

当前从机的初始化/周期/事件入口已从 main 拆到 App/BSP。
`optical_flow_process(now_ms)`、`master_link_process(now_ms)`、`board_inputs_take()` 都由主循环调用。
光流 IRQ 不再解析；TX IRQ 不再修改业务 busy 状态。
共享 `dma_rx` 位于 `Shared/Drivers`，两目标引用同一源文件。
软件 I2C 短事务保留，显示分块、ADC EOC 轮询、UART DMA 避免长时间连续占用。
完整的频率、缓冲所有权、Flash 暂停和诊断计数见 [运行调度](RUNTIME_SCHEDULING.md)。

