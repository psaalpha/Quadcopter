# 测试与验证

## 2026-10-28 记录

两条分支分别构建和测试。Linux WSL Ubuntu 24.04、GCC 13.3、CMake Release：各 15/15 项通过，断言保持启用，主机编译警告视为错误。
Keil 工程、源文件/include、两份工程列表一致性与文档链接检查通过；FreeRTOS 分支另通过固定上游哈希、静态分配、中断与异常归属源码检查。

使用 Arm GNU Toolchain 15.2.Rel1 构建四个完整固件：均成功链接，0 warning / 0 error，生成 ELF/HEX/BIN/map，并校验实际复位/SVC/PendSV/SysTick 向量。

| 分支 | 目标 | Flash 映像字节 | RAM 预算字节 |
|---|---|---:|---:|
| baremetal | Master_MCU | 40168 | 4432 |
| baremetal | Slave_MCU | 31368 | 3096 |
| freertos | Master_MCU | 45364 | 12872 |
| freertos | Slave_MCU | 36440 | 10904 |

应用 Flash 上限 63488B（62KB），其余 2KB 为双页配置；RAM 上限 20480B。
RAM 数字包含静态数据、对齐、所有静态任务栈（如有）、512B heap 预留和 1024B MSP。实际栈高水位与最坏时延尚需实机采样。
构建结果在 `build/firmware-gcc/summary.json`；不同编译器版本会改变体积。

当前免费 Keil ARMCC 链接器存在 32KB 限制，本次完整功能构建以 GNU 成功结果为准，不声明免费 Keil 全目标链接通过。

## 电脑测试覆盖

| 测试 | 主要场景 |
|---|---|
| task_health | 心跳门限、回绕、喂狗条件 |
| crsf_io | 真正驱动的 DMA 分段、CRC、覆盖、硬件错误和重同步 |
| inter_mcu_protocol | v2 / 53 字节、XYZ/积分时间/epoch、字节序、CRC/长度/版本错误 |
| flight_safety | OFF→ON、禁止延迟解锁、失联锁定、故障恢复和计时回绕 |
| flight_controller | 真正业务/姿态/PID/参数联调：调参读回/保存加载、倾角/低压解锁限制、低压不切断飞行输出、导航激活/过期退出、旧帧拒绝、IMU 故障、四电机等值受限试转/租期和校准 |
| pid_control | 角度/角速度、混控、限幅、导航物理单位与重置 |
| yaw_estimator | 最短角误差、三维倾斜补偿、异常磁场拒绝、持久航向参考、陀螺运动拒绝/超时/成功 |
| master_io | 蓝牙有界组帧与统一命令解析、主从 RX、DMA 所有权、立即安全 PWM 与预装载 |
| scheduler | 周期相位、事件合并、积压、时间回绕 |
| slave_io | 光流 checksum/有效性、接收恢复、TX/输入边沿、暂停恢复 |
| slave_app | 采集/发布、20s 校准协调、显示快照、慢爬升高度保留和按键归零 |
| cv0850 | 原生/MSPv2 分段、正负值、校验错误、坏功能和有效标志 |
| flight_parameters | 名称映射、数值解析、非有限/越界拒绝、导航前置、临时字段不保存、序列化 |
| config_journal | 最大 256B 记录，在 145 个擦写/半字/提交边界中断；旧/新完整数据选择、坏 CRC 回退、schema 拒绝 |
| navigation_estimator | 角增量→mm、高度单位、旋转补偿、航向转换、epoch/无效高度、计数回绕 |

Windows 原生仅运行 10 项可移植子集；Linux 运行全部 15 项。寄存器模型针对真正驱动，但不模拟 ARM 指令时序或 FreeRTOS 内核调度。
磁校准算法的物理准确性、原厂传感器驱动和 OSD 字库显示仍需实机验证；电脑测试不代表这些电气行为已验证。

## 实机验证

当前未连接飞控，没有烧录、台架或飞行验证结论。
需要核对 CV-0850 输出模式/单位/安装方向、MPU/磁轴对齐、三轴校准覆盖、真实电压与 ADC 分压、PID/悬停油门、传感器失效恢复、真实 PWM/故障输出和最坏运行时间。
FreeRTOS 另测任务栈高水位与抢占时序；两分支均检查 Flash 保存中断、IWDG 复位、OSD 零值/负数/电压和串口持续负载。
先拆桨执行[安全操作](SAFETY.md)的故障注入，再评估系留与飞行。所有飞行参数沿用原工程基础或保持辅助默认关闭，不把未实测增益当作已经调好的飞行配置。
