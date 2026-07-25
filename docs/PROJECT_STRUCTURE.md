# 项目结构与模块职责

## 仓库总览

```text
Quadcopter/
├── Platform/STM32F1/          公共芯片平台
├── Shared/Protocol/           主从共享协议
├── Master_MCU/                主控固件
│   ├── App/                   应用调度与安全策略
│   ├── BSP/                   板级配置和定时器资源
│   ├── Hardware/              驱动与当前控制算法
│   └── User/                  main 和中断入口
├── Slave_MCU/                 从控固件
│   ├── Hardware/              传感器、显示和外设驱动
│   └── User/                  采集、汇总和发送逻辑
├── tests/host/                硬件无关单元测试
├── tools/                     工程校验和双目标构建
├── docs/                      工程文档
└── .github/workflows/         自动质量检查
```

## Platform

`Platform/STM32F1` 是 Master 和 Slave 的共同基础。

| 目录 | 内容 | 修改约束 |
|---|---|---|
| `CMSIS` | Cortex-M3、启动文件、芯片系统文件 | 只做芯片平台级修改 |
| `SPL` | STM32F10x 标准外设库 | 原则上视为第三方稳定代码 |
| `System` | 当前公共 Delay 服务 | 不得依赖具体飞控业务 |

禁止在 Master 或 Slave 下重新复制这些目录。

## Shared

`Shared/Protocol` 保存主从固件共同编译的纯 C 协议实现。

- 不得包含 `stm32f10x.h`；
- 不得访问寄存器；
- 不得依赖 Master/Slave 私有头文件；
- 必须能被电脑端编译器直接构建；
- 协议字段变化必须更新 `PROTOCOL.md` 和测试。

## Master MCU

### App

| 模块 | 职责 |
|---|---|
| `app_scheduler` | 周期任务通知、消费和 overrun 计数 |
| `flight_safety` | 启动锁、运行、失联和恢复锁状态转换 |

App 表达系统策略，不应直接配置 GPIO、DMA 或 USART。

### BSP

| 模块 | 职责 |
|---|---|
| `board_config.h` | 周期、通道、超时和电机安全常量 |
| `control_timers` | TIM1/TIM2/TIM3 时基与 NVIC 配置 |

BSP 是板级资源的唯一解释层。引脚和定时器调整应先检查 `PINOUT.md`。

### Hardware

当前包含：

- MPU6050 和软件 I2C；
- CRSF 接收；
- Master/Slave 串口链路；
- 蓝牙调参与遥测；
- TIM4 四路 ESC PWM；
- LED 和 IWDG；
- PID、Kalman、姿态辅助代码；
- 若干当前未启用的历史驱动。

注意：当前 `Hardware` 仍同时包含驱动和算法，这是后续需要继续拆分的技术债。

### User

`main.c` 负责：

- 按依赖顺序初始化模块；
- 从调度器领取任务；
- 编排驱动和算法调用；
- 处理必要的中断入口。

不要继续把新的协议实现、参数存储或复杂状态机直接堆入 `main.c`。

## Slave MCU

Slave 当前没有独立 App/BSP 目录，主要由：

- `User/main.c`：20 Hz 数据采集、状态处理和协议发送；
- `Hardware/BMP390*`：气压计适配；
- `Hardware/OpticalFlow*`：光流串口接收；
- `Hardware/QMC5883P*`：磁力计与校准；
- `Hardware/AT7456E*`、`OLED*`：显示；
- 舵机、电池和蜂鸣器的应用初始化。

后续重构从控时，应沿用 Master 已建立的 App/BSP/Hardware 边界，但不要为了目录统一而一次性大规模搬动稳定代码。

## 活跃代码与遗留代码

“出现在 Keil 工程中”不等于“当前运行时已初始化”。例如 Master 的 NRF24L01、OLED 等驱动仍被保留，但当前主路径没有启用。

维护原则：

1. 启用遗留驱动前先完成引脚、DMA、定时器和中断资源审查；
2. 不使用的驱动不要默认假设已经验证；
3. 长期不使用的模块应在独立提交中移出默认产品目标；
4. 不要在清理遗留代码的同时修改飞行控制算法。
