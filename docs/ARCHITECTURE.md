# 架构

## 模块

Master App 的 `flight_controller` 是唯一的业务编排入口，管理安全、遥控映射、命令、健康状态和遥测。
Control 放姿态、导航估计与 PID；Drivers 放设备/通信；BSP 放电机、时钟、LED 和看门狗。
Slave App 管传感器/校准状态机与显示快照；BSP 管 ADC、舵机、按键和告警。
`Shared/Protocol` 是可移植的帧、CV-0850、参数与 Flash 记录算法；`Shared/Scheduling` 放周期/健康辅助；`Shared/Drivers` 依赖 STM32 DMA/时钟；Platform 放芯片库与 Flash 后端。

## 运行模型

两颗 MCU 分别运行 FreeRTOS，使用抢占调度与静态分配，不编译 heap_x.c。

| 任务 | 优先级 | 栈：字/字节 | 职责 |
|---|---:|---:|---|
| Master control | 5 | 1024 / 4096 | 遥控、安全、IMU、PID、传感器解析、命令执行 |
| Master bluetooth | 2 | 640 / 2560 | UART 服务、命令接收、响应与遥测发送 |
| Slave sensors | 4 | 1024 / 4096 | 输入、光流、ADC、50ms 采集/发布、校准 |
| Slave display | 1 | 640 / 2560 | 200ms 快照，每 1ms 至多一个 OSD 字段/OLED 字符 |
| idle | 0 | 128 / 512 | 内核空闲 |

Master 命令和响应队列各长 4，消息按值复制；满时等待下一轮，不在控制任务阻塞。遥测是长 1 的 128B 最新值邮箱；Slave 显示是长 1 的结构体邮箱。
控制任务独占全部控制状态；蓝牙任务只传命令/字节，显示任务只消费快照。不存在跨任务共享传感器总线，当前无需 mutex。
SysTick 为内核 1kHz 时基；SVC/PendSV/SysTick 由官方 ARM_CM3 端口拥有。TIM2 优先级 6，DMA/USART 7 或 8，符合 FromISR >=5 要求；NVIC 为 group4。
`master_*_stack_free`、`slave_*_stack_free` 为栈剩余字数，`*_max_step_cycles` 为最长服务步骤核周期，均需上板采集。

## 周期与数据

| 工作 | 周期 |
|---|---:|
| Master IMU / 角速度 PID | 2ms |
| Master RC | 5ms |
| Master 姿态外环 | 10ms |
| Master 遥测页 | 100ms |
| Slave 传感器与主从发布 | 50ms |
| Slave 显示快照 | 200ms |
| 电机 PWM / 舵机 PWM | 20ms |

主控数据路径：CRSF→安全/目标；MPU→姿态→串级 PID→混控→PWM；从控新帧→磁向量融合/导航估计→辅助 PID；蓝牙→类型化命令→参数表→控制器。
从控 CV-0850 每帧累计光流角增量和积分时间，发布时复制累计值；主控差分求出该窗口运动，失效 epoch 改变则重新建基准。
辅助高度使用测距×cos(roll)×cos(pitch)，水平位移为 mm，PID 内转 cm。BMP390 高度保持启动/按键参考，慢爬升不会被当作零点漂移消除。
磁力计每个新样本只融合一次；三轴校准在从控执行，倾斜和异常检测在主控执行。

## I/O 与维护

CRSF 用 512B RX 环和最多 256B 批次；其余 RX 为 256B 环/64B 批次。IDLE/HT/TC 只记录事件，组帧在业务循环。
UART/DMA 异常、覆盖和半帧超时走重同步；TX busy 不覆盖缓冲，20ms 未完成会回收，服务不无限等待。
OLED 每次只改一个不同字符；OSD 每次只改一个字段。两者显示本地实测高度、测距/质量、航向、温度、电压和校准倒计时，不显示固定的飞行解锁状态。
所有数据显示使用有界、无需堆分配的格式化。

每块板的末两页 Flash 交替保存配置，schema/长度/CRC/代数和最后提交标记用于选取最近完整记录。
主控停机保存时进入维护窗口，从控保存时等 TX 回收、暂停光流再恢复。Flash 中断屏蔽期间经过的时间由 DWT 在下次 TIM2 补记；结束后重新建立控制周期基准。
DWT 32 位在 72MHz 约 59.6s 回绕，必须持续频繁读取；运行期换时钟和断点暂停不纳入时间保证。

两端应用限制为 62KB Flash，给两页配置留空间；RAM 为 20KB，详见[构建](BUILD.md)和[测试记录](TESTING.md)。
