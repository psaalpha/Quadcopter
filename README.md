# Quadcopter

基于双 STM32F103C8T6 的四旋翼飞控固件，使用 C、STM32 SPL 和 Keil ARMCC5。
当前 `baremetal` 分支持续完善裸机实现，采用定时器事件与主循环协作式调度；`freertos` 分支维护 FreeRTOS 实现。

主控负责遥控接收、姿态估计、串级 PID 和电机输出；从控负责传感器采集、显示、电池监测和数据发送。

## 功能

- MPU6050 姿态估计，100Hz 角度外环、500Hz 角速度内环和四路 50Hz PWM。
- TIM2 毫秒时基与周期事件，通信解析、传感器服务和控制计算在主循环执行。
- CRSF 遥控接收：420000 baud、DMA 接收、CRC8 校验和通道解包。
- 主从通信：115200 baud、41 字节固定帧、版本、序号和 CRC16。
- 上电低油门锁、300ms 遥控失联保护和看门狗。
- BMP390、QMC5883P 和光流采集，以及高度/光流辅助控制接口。

辅助 PID 默认零增益，传感器方向、控制参数和实际运行时序仍需验证。构建与硬件验证状态见[测试与验证](docs/TESTING.md)。

## 目录

```text
Master_MCU/       主控固件
Slave_MCU/        从控固件
Shared/           共享协议、调度和 DMA 辅助模块
Platform/STM32F1/ CMSIS、SPL 和公共延时
tests/host/       主机测试与寄存器模型
tools/            工程校验与固件构建脚本
docs/             工程文档
```

## 构建

Linux 主机完整回归：

```sh
python tools/validate_project.py
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

- [架构](docs/ARCHITECTURE.md)：模块职责、裸机调度、周期、中断和数据所有权。
- [构建](docs/BUILD.md)：环境、测试命令、Keil 编译和成对烧录。
- [测试与验证](docs/TESTING.md)：测试覆盖、已有记录和待验证项。
- [硬件资源](docs/PINOUT.md)：引脚、定时器和 DMA 分配。
- [主从协议](docs/PROTOCOL.md)：41 字节帧格式和兼容约定。
- [安全状态](docs/SAFETY.md)：启动锁、失联恢复和已知限制。

## 分支

| 分支 | 用途 |
|---|---|
| `baremetal` | 持续完善裸机调度、驱动、控制和通信 |
| `freertos` | 维护 FreeRTOS 任务、通知、队列与实时系统实现；默认分支 |

两条分支分别维护各自运行模型。通用修复按需同步；独立功能或修复使用临时分支，完成后合入对应版本并删除。
