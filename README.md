# Quadcopter

双 STM32F103C8T6 四旋翼固件，C、STM32 SPL，支持 GNU ARM 与 Keil ARMCC5。
当前 `freertos` 分支采用FreeRTOS 静态任务、通知和队列。仓库只维护 `baremetal` 和 `freertos` 两条分支，默认分支为 `freertos`。

## 功能

- MPU6050 姿态估计、静止零偏校准，100Hz 角度环、500Hz 角速度环和四路 50Hz ESC PWM。
- CRSF DMA 接收/校验、CH8 ARM/DISARM、失联与 IMU 故障保护、电池解锁检查。
- CV-0850 原生/MSPv2 校验与累计、测距和物理位移换算、高度/水平位置辅助控制、失效恢复。
- QMC5883P 三轴偏移/比例校准、磁航向倾斜补偿、异常拒绝与航向归零。
- BMP390 相对高度与按键归零，电压采样/蜂鸣器、舵机、OLED/OSD 分步刷新。
- 蓝牙完整调参、读取/保存/加载/默认值、状态/电机/导航遥测和受限无桨试转。
- 主从 v2 / 53 字节 / CRC16；双页 Flash 参数保存，写入中断保留旧记录。

两条分支均完成电脑回归和主从固件构建。实机轴向、尺度、增益、时延和飞行效果尚未验证；导航默认关闭，辅助增益默认零，需校准调参后启用。

## 目录与使用

`Master_MCU/` 控制、遥控和电机；`Slave_MCU/` 传感器、显示与电压；`Shared/` 公共协议、参数与调度；`Platform/` 芯片平台；`tests/host/` 回归；`tools/` 构建检查；`docs/` 六份使用文档。

```sh
python tools/validate_project.py
python tools/build_gcc.py
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

## 文档

- [架构](docs/ARCHITECTURE.md)：模块、周期、数据所有权。
- [构建](docs/BUILD.md)：工具链、固件与烧录。
- [测试记录](docs/TESTING.md)：覆盖、资源和实机待验证项。
- [硬件资源](docs/PINOUT.md)：引脚、通道、定时器与 DMA。
- [通信与参数](docs/PROTOCOL.md)：53 字节帧、CV-0850、蓝牙指令和遥测。
- [安全操作](docs/SAFETY.md)：CH8 解锁、校准、辅助模式、试转和故障行为。

## 分支

| 分支 | 用途 |
|---|---|
| `baremetal` | 持续完善裸机驱动、控制与通信，不包含 RTOS 内核 |
| `freertos` | 同一功能集的 FreeRTOS 任务实现，默认分支 |

通用功能与修复保持一致，各自保留自己的运行入口。
