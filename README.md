# Quadcopter

基于双 STM32F103C8T6 和 FreeRTOS 的四旋翼飞控固件，使用 C、STM32 SPL 和 Keil ARMCC5。
当前 `freertos` 分支维护 FreeRTOS 实现；`baremetal` 分支持续完善裸机版本。

主控负责遥控接收、姿态估计、串级 PID 和电机输出；从控负责传感器采集、显示、电池监测和数据发送。

## 功能

- MPU6050 姿态估计，100Hz 角度外环、500Hz 角速度内环和四路 50Hz PWM。
- FreeRTOS 静态任务与队列，通过任务通知处理定时器和 DMA 事件。
- CRSF 遥控接收：420000 baud、DMA 环形缓冲、CRC8 校验和异常重同步。
- 主从通信：115200 baud、41 字节固定帧、版本、序号和 CRC16。
- 上电低油门锁、300ms 遥控失联保护、任务心跳看门狗。
- BMP390、QMC5883P 和光流采集，以及高度/光流辅助控制接口。

当前版本已有双目标编译和主机回归记录；尚未完成上板、台架和飞行验证。辅助 PID 默认零增益，实际周期、栈余量和传感器方向仍需测量。详见[测试与验证](docs/TESTING.md)。

## 目录

```text
Master_MCU/       主控固件
Slave_MCU/        从控固件
Shared/           共享协议、调度和 DMA 辅助模块
Platform/         STM32 平台代码与 FreeRTOS
tests/host/       主机测试与寄存器模型
tools/            工程校验与固件构建脚本
docs/             工程文档
```

## 构建

仓库根目录执行结构与 RTOS 源码检查：

```sh
python tools/validate_project.py
python tools/check_freertos.py
```

Linux 主机完整回归：

```sh
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Windows 双目标固件构建：

```powershell
.\tools\build_firmware.ps1
```

工具链设置、Windows 主机测试和烧录步骤见[构建说明](docs/BUILD.md)。

## 文档

- [架构](docs/ARCHITECTURE.md)：模块职责、任务、周期、中断和数据所有权。
- [构建](docs/BUILD.md)：环境、测试命令、Keil 编译和成对烧录。
- [测试与验证](docs/TESTING.md)：测试覆盖、构建记录、资源占用和待验证项。
- [硬件资源](docs/PINOUT.md)：引脚、定时器和 DMA 分配。
- [主从协议](docs/PROTOCOL.md)：41 字节帧格式和兼容约定。
- [安全状态](docs/SAFETY.md)：启动锁、失联恢复、故障停机和已知限制。

## 分支

| 分支 | 用途 |
|---|---|
| `baremetal` | 持续完善裸机调度、驱动、控制和通信 |
| `freertos` | 维护 FreeRTOS 任务、通知、队列与实时系统实现；默认分支 |

两条分支分别维护各自运行模型。通用修复按需同步；独立功能或修复使用临时分支，完成后合入对应版本并删除。
