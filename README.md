# Quadcopter STM32 Flight Controller

当前分支已迁移到 FreeRTOS。学习入口见 [裸机到 RTOS 映射](docs/FREERTOS_MIGRATION.md)，
构建结果与硬件验证边界见 [验证记录](docs/FREERTOS_VALIDATION.md)。

这是一个双 STM32F103C8T6 飞控工程：

- `Master_MCU` 负责遥控输入、姿态解算、串级 PID、安全状态和四路电机输出。
- `Slave_MCU` 负责气压计、光流、磁力计、电池监测、舵机、OLED/OSD，以及向主控汇总传感器数据。

`feature/freertos-study-migration` 从远端 `codex/resume-study-spl` 最新提交
`3d4a73d9be58bc02c4fcd5acf1b60bccdce97213` 独立派生。主控控制/蓝牙、从控采集/显示
采用静态 FreeRTOS 任务、通知与队列，保留 SPL、Keil、原算法和41字节协议。
本分支尚未上板验证；辅助PID默认零增益，不能用于宣称可安全实飞。

首次接手项目请从 [工程文档索引](docs/README.md) 开始，里面提供按开发角色和学习顺序组织的阅读路径。

## 工程结构

```text
Master_MCU/  Core / App / BSP / Control / Drivers
Slave_MCU/   Core / App / BSP / Drivers
Shared/      Protocol / Scheduling / Drivers
Platform/STM32F1/  CMSIS / SPL / System
tests/host/  主机单元测试和寄存器模型
tools/       工程检查及 Keil 双目标构建脚本
docs/        架构、命名、协议、引脚、调度和学习文档
```

详细模块归属和重命名说明见 [项目结构与命名规范](docs/PROJECT_STRUCTURE.md)。BMP390 适配层与 Bosch 原厂文件统一放在 `Slave_MCU/Drivers/Sensors/BMP390/`。

## 快速验证

电脑端检查：

```powershell
python tools/validate_project.py
python tools/check_freertos.py
cmake -S . -B build/host -G "MinGW Makefiles"
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Keil/ARMCC 双目标构建：

```powershell
.\tools\build_firmware.ps1
```

工程默认使用 ARMCC5 `-O2` 和 MicroLIB，两目标已在现有32KB免费链接器下构建。
从控余量有限，新增功能后必须重新检查链接图；64KB Flash物理容量不等于32KB工具授权上限。
主机测试也不模拟RTOS真实调度、DMA电气时序或飞控硬件。

如果 Keil 不在默认路径：

```powershell
.\tools\build_firmware.ps1 -KeilPath "D:\Keil_v5\UV4\UV4.exe"
```

## 重要安全说明

- 上电、遥控失联和重连后都必须先收到有效的低油门帧，电机输出才会放行。
- 遥控连续 300 ms 没有有效 CRSF 帧会进入失联保护并立即写入四路最小输出。
- 当前尚未绑定专用 ARM/DISARM 开关。低油门解锁只是基础保护，不能替代正式的飞行解锁流程。
- 主从通信协议已经升级，主控和从控必须成对烧录同一版本。

在连接桨叶前，必须先完成无桨台架验证。更详细的限制和测试要求见 [安全说明](docs/SAFETY.md)。

## 文档

- [简历项目复习导读（本分支从这里开始）](docs/RESUME_STUDY_GUIDE.md)
- [develop 嵌入式工程化学习指南](docs/DEVELOP_ENGINEERING_LEARNING_GUIDE.md)
- [工程文档索引与学习路径](docs/README.md)
- [项目结构与模块职责](docs/PROJECT_STRUCTURE.md)
- [总体架构](docs/ARCHITECTURE.md)
- [驱动接口规范](docs/DRIVER_API.md)
- [开发维护指南](docs/DEVELOPMENT_GUIDE.md)
- [构建与验证](docs/BUILD.md)
- [自动测试说明](docs/TESTING.md)
- [硬件资源与引脚](docs/PINOUT.md)
- [主从通信协议](docs/PROTOCOL.md)
- [安全状态机](docs/SAFETY.md)
- [运行维护手册](docs/MAINTENANCE.md)
- [固件发布流程](docs/RELEASE.md)
- [后续演进路线](docs/ROADMAP.md)
- [协作规范](CONTRIBUTING.md)

