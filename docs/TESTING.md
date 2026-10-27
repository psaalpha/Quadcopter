# 测试与验证

以下记录区分主机逻辑、固件构建和实际硬件。现有构建记录不代表已完成台架或飞行验证。

## 检查记录（2026-10-27）

- WSL Ubuntu 24.04、GCC 13.3.0、CMake Release 构建成功，10/10 项 CTest 全部通过。
- 工程结构、两个 Keil 工程引用和文档链接校验通过。
- FreeRTOS 固定内核、静态分配、中断与异常向量源码检查通过。
- 本次整理未修改固件运行代码；未重新执行 Keil 链接或硬件测试。

## 测试覆盖

| 测试 | 覆盖 |
|---|---|
| task_health | 主从任务心跳、喂狗门限与时基回绕 |
| crsf_io | DMA 分段、CRC、环形覆盖、错误与重同步 |
| inter_mcu_protocol | 41 字节协议、字节序、CRC 和异常帧 |
| flight_safety | 启动锁、300ms 失联、重连低油门锁与时间回绕 |
| pid_control | PID 限幅、混控、辅助输出与重置 |
| yaw_estimator | 磁力计首帧对齐、角度回绕与无效输入 |
| master_io | 主控串口、DMA 所有权、遥测与 PWM 寄存器模型 |
| scheduler | 周期事件、合并、积压与时间回绕 |
| slave_io | 光流接收、DMA 发送、输入边沿与缓冲回收 |
| slave_app | 采集、发布、校准、显示及服务协作 |

Linux 构建运行全部 10 项，Windows 原生构建运行其中 5 项可移植测试。
寄存器模型不验证真实中断延迟、DMA 电气时序或 FreeRTOS 抢占行为。
断言在 Release 中保持启用，CI 把主机编译警告视为错误。

## 已有构建与回归记录（2026-10-16）

- Keil MDK、ARM Compiler 5.06 update 7 build 960，两目标编译及链接均0 error / 0 warning。
- 默认工程使用ARMCC `-O2`、MicroLIB、C99和SPL，无内核运行期动态分配。
- Linux WSL GCC 13.3.0、CMake/CTest Release：10项回归全部通过，编译警告视为错误。
- `validate_project.py`：双Keil源文件/选项列表、include、共享协议、校准页边界、文档链接检查通过。
- `check_freertos.py`：固定内核文件校验、IRQ数值、异常向量归属、无heap_x.c、延时不占用SysTick，
  以及本地Keil map的Flash/RAM预算检查通过。
- `git diff --check`无空白错误；Git在Windows提示的LF/CRLF转换不代表编译失败。
- 该次 RTOS 集成保持协议、PID/姿态算法与纯安全状态机实现。

## Flash/RAM 与链接器预算（2026-10-16）

按ST数据手册，F103C8为64KB Flash、20KB SRAM，不能按部分板卡可能存在的额外Flash扩容。
从控继续预留最后1KB `0x0800FC00..0x0800FFFF`，应用IROM上限为63KB。

| 最终链接图指标 / 字节 | Master | Slave |
|---|---:|---:|
| Code | 24,172 | 30,280 |
| RO data | 508 | 2,016 |
| RW data 未压缩 | 684 | 344 |
| ZI data | 9,668 | 10,696 |
| RO合计 | 24,680 | 32,296 |
| ROM合计 含压缩初值 | 24,784 | 32,340 |
| Load Region未压缩尺寸 保守预算 | 25,364 | 32,640 |
| RAM RW加ZI | 10,352 | 11,040 |
| SRAM余量 相对20,480 | 10,128 | 9,440 |
| 相对32,768的保守余量 | 7,404 | 128 |

RAM包含静态任务栈、TCB、队列、DMA buffer、1024B启动/异常MSP和原512B C库heap保留区。
静态内存总量合格不等于每个任务的栈深足够；PSP高水位和MSP嵌套必须上板量测。
从控当前虽已通过免费版链接，但保守余量仅128B。换编译器、改变优化或增加代码后
可能再次超限；64KB 芯片容量与工具的 32KB 限制分别生效。

历史裸机和完整 C 库试编曾超过 32KB 工具限制；最终配置为 `-O2` 与 MicroLIB。
失败记录与最终成功构建分别保存在原始日志中，MicroLIB 硬件路径仍待复核。

原始文本证据在 [validation目录](validation)。链接图摘要包含ROM/RAM及SVC、PendSV、SysTick
来自官方port.o的符号信息。临时试编失败日志只说明当时配置的限制，不是最终源码可用性的证据。

复现命令见[构建](BUILD.md)。GitHub Actions 不含 Keil 链接，主机测试不运行真实 Cortex-M 调度器。

## 尚未完成

- 未烧录两块实际F103，未测2ms控制周期、最坏执行时间、ISR到任务延迟或PWM电气行为。
- 未测任务PSP/MSP余量、饥饿后的真实IWDG复位、栈hook/assert/CPU异常最小输出。
- 未验证MicroLIB下的硬件显示与软浮点路径，未做DMA高负载、Flash校准和输入边沿台架测试。
- 未确认光流模块校验规则及物理量尺度，未改变导航零增益默认值或增加正式ARM开关。
- 无桨台架、受控飞行和调参均未执行；当前版本不能标记为可安全实飞。

台架验证按本文末节执行，运行模型见[架构](ARCHITECTURE.md)。

## 台架验证项目

1. 成对烧录两端，记录芯片、时钟、固件提交和复位原因；检查启动锁与四路最小 PWM。
2. 测量 2ms 内环、10ms 外环、ISR 到任务延迟、I2C 时序和四路 50Hz PWM；通信与显示同时满载时记录抖动。
3. 验证高油门启动、低油门解锁、300ms 失联及高油门重连锁；注入控制积压/停滞并检查输出和复位。
4. 阻塞后台任务，检查任务心跳、IWDG、栈 hook、assert 和异常路径。
5. 验证 DMA 半帧、粘帧、无 IDLE 连续流、溢出、TE/UART 错误、发送 busy 和 20ms 超时回收。
6. 检查校准与 Flash 保存期间的时基、光流服务、ADC、输入脉冲和舵机；验证 200ms 导航超时。
7. 量测各任务 PSP 水位、MSP 嵌套余量与最坏执行时间；确认 MicroLIB 显示/软浮点路径。

传感器方向、光流物理尺度、辅助 PID 参数和正式 ARM/DISARM 流程仍是后续工作。结果应记录实测环境和原始数据。
