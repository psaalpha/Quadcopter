# Quadcopter STM32 Flight Controller

这是一个双 STM32F103C8T6 飞控工程：

- `Master_MCU` 负责遥控输入、姿态解算、串级 PID、安全状态和四路电机输出。
- `Slave_MCU` 负责气压计、光流、磁力计、电池监测、舵机、OLED/OSD，以及向主控汇总传感器数据。

当前 `develop` 分支已经完成基础工程化改造：公共平台代码去重、主从协议版本化、主控实时任务调度、安全状态机、电脑端单元测试、项目结构校验和 GitHub 自动检查。

## 工程结构

```text
.
├── Platform/STM32F1/       # 两颗 MCU 共用的 CMSIS、SPL 和系统服务
├── Shared/Protocol/        # 与硬件无关的主从通信协议
├── Master_MCU/
│   ├── App/                # 调度与飞行安全策略
│   ├── BSP/                # 板级配置和控制定时器
│   ├── Hardware/           # 主控外设驱动与控制算法
│   └── User/               # 启动入口和中断入口
├── Slave_MCU/
│   ├── Hardware/           # 传感器、显示和外设驱动
│   └── User/               # 从控应用入口
├── tests/host/             # 可在电脑上运行的 C 单元测试
├── tools/                  # 构建和结构校验脚本
└── docs/                   # 架构、协议、引脚、安全和构建说明
```

## 快速验证

电脑端检查：

```powershell
python tools/validate_project.py
cmake -S . -B build/host -G "MinGW Makefiles"
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Keil/ARMCC 双目标构建：

```powershell
.\tools\build_firmware.ps1
```

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

- [总体架构](docs/ARCHITECTURE.md)
- [构建与验证](docs/BUILD.md)
- [硬件资源与引脚](docs/PINOUT.md)
- [主从通信协议](docs/PROTOCOL.md)
- [安全状态机](docs/SAFETY.md)
- [后续演进路线](docs/ROADMAP.md)
- [协作规范](CONTRIBUTING.md)
