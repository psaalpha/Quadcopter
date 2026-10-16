# 从裸机多速率调度到 FreeRTOS

本分支是双 STM32F103C8T6 的学习迁移版本，沿用 SPL、Keil ARMCC5、引脚和原有算法。
已完成的软件编译与回归不代表硬件时序或飞行安全已经验证。请先阅读
[本次验证记录](FREERTOS_VALIDATION.md)，再按文末的无桨测试步骤学习。

## 1 先看原来怎样运行

原分支 `codex/resume-study-spl` 在 `3d4a73d9` 用 TIM2 每 1ms 发布周期标志，
主循环依次取走标志。IMU 和角速度内环每 2ms 执行，RC 每 5ms，姿态外环每 10ms，
主控遥测每 100ms；从控每 50ms 采集/发包、每 200ms 准备显示。

这种结构已把长等待改成状态机，适合做最小侵入迁移。此次保留 `periodic_tasks` 的
周期相位、合并和超期计数，增加内核真正的任务栈、抢占与阻塞，不把所有函数各拆一个任务。
采样、姿态和 PID 强相关，分到不同任务反而会引入跨周期数据与锁竞争。

| 原裸机职责 | 本分支执行者 | 唤醒/周期 | 任务优先级 | 栈字/字节 |
|---|---|---|---:|---:|
| 主控安全、RC、IMU、PID、混控、主从RX | `master_control_task` | TIM2每1ms及RX事件；内部保留2/5/10ms相位 | 5 | 768 / 3072 |
| 蓝牙文本解析、调参输入、遥测发送 | `master_background_task` | 每轮后 `vTaskDelay(1)`；遥测邮箱100ms更新 | 2 | 512 / 2048 |
| 从控输入、光流、ADC、传感器、主从TX、校准 | `service_task` | TIM2每1ms及DMA/EXTI事件；传感器/发包50ms | 4 | 1024 / 4096 |
| OLED、OSD和浮点格式化 | `display_task` | 200ms快照；每1ms至多一块显示 | 1 | 640 / 2560 |
| 内核idle | 内核创建的静态任务 | 没有其他Ready任务时 | 0 | 128 / 512 |

两块板分别运行自己的调度器，任务优先级不跨 MCU 比较。任务数值越大越优先，
NVIC 数值越小越紧急，两者不要混淆。关闭同级时间片，因为当前业务任务均有不同优先级。

## 2 从中断到任务到底发生了什么

主控阅读顺序：`Core/main.c` 末尾 `TIM2_IRQHandler` →
`Platform/FreeRTOS/Config/rtos_support.c` → `master_control_task`。

1. TIM2清更新标志，DWT核周期差换算实际毫秒，推进原周期调度器。
2. `xTaskNotifyFromISR(..., eSetBits, ...)` 把TICK/IO位写入目标任务通知值。
3. 若任务因 `xTaskNotifyWait` 阻塞，内核将它移回Ready链表。
4. `portYIELD_FROM_ISR` 挂起PendSV。真正切换在异常退出后进行，ISR不调用PID。
5. PendSV保存旧任务的R4-R11与PSP，选择当前最高优先级Ready任务，恢复新任务。
6. `xTaskNotifyWait` 返回；控制任务先检查安全与超期，再消费已到期的工作。

通知位是可合并的门铃，不是数据队列：连续十次通知可能只看到一个IO位。
实际字节仍在DMA环形缓冲，周期数量与超期仍由 `periodic_tasks` 记录，GPIO校准边沿历史
仍由驱动保存。因此不会把通知位数量误当成串口帧数或输入脉冲数。

已有驱动采用“先取走并清除pending，再执行函数”，执行中到来的事件留给下一轮。
DMA中断先记账再通知。启动过程中先到来的IRQ不会调用尚未启动的调度器；第一轮tick
会处理驱动已保留的事件。IDLE表示串口间隙，绝不表示一帧的边界。

## 3 为什么保留 TIM2 又用 SysTick

SysTick由官方ARM_CM3端口设置为1kHz，用于内核超时和低优先级任务的 `vTaskDelay`。
TIM2沿用原来的周期相位与DWT真实时间记账，避免改变所有驱动时间接口。
它们都会产生中断，所以必须量测开销，不能宣称“两份1ms时基没有成本”。

原 `Delay_us` 改写SysTick的LOAD/VAL/CTRL，会使调度器失效，故改用DWT CYCCNT。
每次调用的起点保存在当前任务自己的栈上，不清零全局计数器，不关中断忙等。
1us的I2C延时仍为短忙等；中断抢占可能拉长电平时间，需逻辑分析仪确认从设备允许。
长毫秒延时只在调度器启动前的设备初始化使用，运行期继续用非阻塞状态机。

Flash擦写仍会短时禁止中断，SysTick挂起位不能累计丢失的所有tick，
因此不把内核tick用于校准10秒或安全超时。TIM2恢复后通过DWT补记真实经过时间；
内核延时只承担后台让出CPU。没有实现tick补偿、tickless或睡眠模式。
CYCCNT约59.6秒回绕，必须持续运行并频繁读取；调试暂停/运行时换时钟不在支持范围。

## 4 共享数据用所有权和队列解决

主控控制任务独占RC映射、安全状态、姿态估计、PID及电机CCR。主从解析也在同一任务，
所以 `slave_sensor_data.updated` 的快照读取没有跨任务写入者。
蓝牙任务只解析文本，发送 `tuning_command_t` 值到长度4 FIFO；控制任务每次最多取一条，
队列超时参数为0，不等后台。队列满时后台不继续消费新命令，输入ring持续满则按原驱动
溢出策略丢弃并重新同步，不保证在无限输入下保留所有调参命令。

主控遥测用长度1的float邮箱；从控显示用长度1的完整结构体邮箱。
`xQueueOverwrite` 表示旧快照可以舍弃，新快照优先。内核复制数据，不传指向生产者栈的指针。
显示任务独占显示缓冲和OLED/SPI2，不再直接读取正在更新的气压、磁力计或光流全局状态。

本次没有引入mutex/二值信号量：任务通知已能唤醒单个消费者，队列已承载跨任务数据，
各外设又有单一任务所有者。无共享竞争就不需要额外的锁。
若今后两任务共用同一个I2C/SPI外设，应增加带优先级继承的mutex并约束持锁时间，
不能用 `volatile` 代替同步，也不能在持锁期间等待慢速显示或做Flash擦写。

MicroLIB是为体积选择的小型C库，不应视为所有C库调用都已具备RTOS线程安全保证。
当前主控运行期不使用stdio；从控运行期只有显示任务调用 `snprintf`，没有运行期malloc、
共享FILE或多任务printf。数学函数仍使用原软浮点实现，错误域、性能和显示格式需板上核验。

## 5 中断优先级的具体约束

两个MCU均 `NVIC_PriorityGroup_4`，4位抢占优先级、0位子优先级。
所有子优先级设置为0，禁止模块偷偷改回Group_2。

| IRQ | SPL未移位数值 | 是否可调用FromISR |
|---|---:|---|
| TIM2 | 6 | 是 |
| 主控CRSF USART2/DMA6、主从USART3/DMA3 | 7 | 是 |
| 从控光流USART1/DMA5、主从TX DMA7 | 7 | 是 |
| 主控蓝牙USART1/DMA4/5、从控EXTI | 8 | 是；蓝牙保持驱动pending轮询 |
| PendSV、SysTick | 15 | 内核拥有 |
| 预留0..4 | 未使用 | 禁止调用任何FreeRTOS API |

`configMAX_SYSCALL_INTERRUPT_PRIORITY=0x50` 是写入BASEPRI的已左移值；
SPL配置里的6、7、8是未移位值。ARMCC内联汇编读取该宏，使用整数字面量避免汇编表达式问题。
`configASSERT` 保持开启，官方端口会检查使用FromISR的IRQ优先级与优先级组。
旧驱动PRIMASK临界区依旧只包围事件计数与游标采样，恢复进入前的屏蔽状态；
它会屏蔽所有可屏蔽IRQ，必须短于接收ring完整绕圈时间。

## 6 控制路径与新增故障策略

2ms固定dt、100Hz外环、500Hz内环、四路50Hz TIM4 PWM、导航默认零增益均保持。
传感器读取、PID和CCR写入仍同步执行于最高业务优先级，不在中途等待队列、串口或互斥锁。
这不等于所有代码瞬时完成：软件I2C与软浮点仍占用CPU，必须量测最坏执行时间。

原状态机继续要求上电/失联恢复先低油门，连续300ms无有效CRSF帧立即写最小输出，
导航数据200ms失效回到手动。超时检查移到新帧接收前后，防止旧ACTIVE状态被高油门新帧续期。

新增两道保守的实验保护：IMU周期出现未消费积压时进入故障停机；TIM2发现控制步骤
超过10ms没有完成也进入同一路径。故障函数禁止中断、写最小PWM、不返回旧任务、不喂狗，
避免“ISR停机后被原控制任务再次写回油门”。需硬件测试触发与复位行为，不能当作航空级安全设计。

主控仅在控制和蓝牙心跳都在100ms内更新时喂IWDG；从控采集和显示心跳门限为250ms。
看门狗参数沿用原板级配置：主控LSI/8、重装499，名义约100ms；从控LSI/64、重装625，
名义约1秒。LSI有误差，这些不是精密时间。主从是各自本地IWDG，不新增未实现的互喂协议。
从控链路异常继续由原数据新鲜度/导航超时处理，不擅自把从控失联改成整机RC失联。

CRSF ring由128B增到512B，420000baud满线约12.2ms绕一圈，每5ms最多解析256B。
新增HT/TC绕圈计数、IDLE/错误通知，溢出丢弃不确定字节，TE/UART错误重置接收再同步。
原CRC8和16×11bit解包不变。其他RX保持256B及64B有界批次。
从控41字节协议、版本、序号、小端字段和CRC16文件完全未修改；TX仍busy即跳过、DMA完成后
由任务回收缓冲、20ms异常超时，不能覆盖DMA拥有的帧。光流模块校验规则未确认的限制也仍保留。

## 7 栈 堆 与学习内核源码

`configSUPPORT_DYNAMIC_ALLOCATION=0`，不编译heap_x.c。所有TCB、任务栈、队列存储、idle栈
都是静态对象，计入链接图ZI区域。启动文件原有MSP=1024B，C库heap=512B，
它们与FreeRTOS任务的PSP栈是不同资源；应用未在运行期申请堆内存。

开启栈溢出检查模式2及故障hook。`master_*_stack_free`、`slave_*_stack_free` 是剩余32位字，
控制/采集水位每秒采样；后台自行采样。`*_max_step_cycles` 是一次服务循环的最大核周期，
包含ISR抢占时间，除以72得到微秒。它不是一次IMU专用WCET，也不能由编译结果推导实际时间。
需分别测量采样起点、内环结束和发布抖动；MSP还需另测中断嵌套余量。

学习顺序建议：先读项目中文注释，再读官方端口 `port.c` 的SVC/PendSV/SysTick，
然后读 `tasks.c` 的 `xTaskNotifyWait` / `xTaskGenericNotifyFromISR` / `vTaskSwitchContext`，
最后读 `queue.c` 的发送/接收和阻塞链表操作。上游源码保持原样，中文解释集中在调用边界，
以便升级依赖时逐文件校验差异。

参考：

- [FreeRTOS Cortex-M 中断优先级说明](https://www.freertos.org/Documentation/02-Kernel/03-Supported-devices/04-Demos/ARM-Cortex/RTOS-Cortex-M3-M4)
- [固定内核版本](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/V11.1.0)
- [STM32F103x8 数据手册](https://www.st.com/resource/en/datasheet/stm32f103c8.pdf)
- [Keil Target 与 MicroLIB 设置](https://www.keil.com/support/man/docs/uv4/uv4_dg_adstarg.asp)

## 8 尚需完成的无桨验证

1. 两块板配对烧录，记录具体芯片、时钟、固件提交和复位原因；确认启动锁及所有PWM安全电平。
2. 用示波器/逻辑分析仪测2ms内环开始/结束、10ms外环、ISR到任务延迟、I2C时序和50Hz四路PWM；
   在满速CRSF、蓝牙噪声、连续主从包、显示刷新同时运行时测最大抖动与CPU余量。
3. 测上电高油门、低油门解锁、300ms失联、重连高油门锁定；触发IMU积压和控制停滞，确认
   最小输出不会被任务恢复覆盖，IWDG按预期复位。
4. 阻塞蓝牙/显示任务，检查心跳门限导致停止喂狗；触发栈hook、assert和总线故障。
5. 复测DMA半帧、粘帧、无IDLE、整圈溢出、TE/UART错误、TX busy和20ms超时回收。
6. 测10秒校准、Flash页边界、保存期间光流暂停/恢复、TIM2真实时间补记、后台tick延迟；
   检查200ms导航数据过期、输入脉冲、去抖、ADC超时与舵机位置。
7. 记录两任务PSP水位和MSP余量，验证MicroLIB显示/软浮点路径，再考虑是否需要调整栈或驱动。

这些步骤尚未执行；本分支不能标记为可安全实飞。
