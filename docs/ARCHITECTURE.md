# 架构

## 模块

Master App 的 `flight_controller` 是唯一的业务编排入口，管理安全、遥控映射、命令、健康状态和遥测。
Control 放姿态、导航估计与 PID；Drivers 放设备/通信；BSP 放电机、时钟、LED 和看门狗。
Slave App 管传感器/校准状态机与显示快照；BSP 管 ADC、舵机、按键和告警。
`Shared/Protocol` 是可移植的帧、CV-0850、参数与 Flash 记录算法；`Shared/Scheduling` 放周期/健康辅助；`Shared/Drivers` 依赖 STM32 DMA/时钟；Platform 放芯片库与 Flash 后端。

## 运行模型

两颗 MCU 各自采用一个裸机循环；TIM2 ISR 记录周期事件，主循环按固定顺序消费，不在 ISR 读传感器或刷新显示。
Master 循环独占控制状态、命令和 UART 发送；Slave 循环顺序处理输入、采集、校准和一小步显示。
无 FreeRTOS 内核、任务、队列或 RTOS 工程文件。SysTick 不参与业务调度；公共延时和真实经过时间由 DWT 计数。
Master 的 `master_max_step_cycles` 可测单轮最坏周期；周期积压和中断停滞保护与 RTOS 版本一致。

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
