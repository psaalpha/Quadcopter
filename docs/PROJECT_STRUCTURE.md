# 项目结构与命名规范

本页对应 `codex/resume-study-spl` 的当前布局。主从控采用相同职责命名；只建立实际需要的层，不为从控建立空的 `Control`。

## 目录总览

```text
Master_MCU/
  Core/                 main.c、STM32 中断支持和 SPL 配置
  App/                  app_scheduler、flight_safety
  BSP/                  board_config、control_timers、motor_pwm、board_led、watchdog
  Control/              attitude_estimator、kalman_filter、pid_controller
  Drivers/
    Bus/                soft_i2c
    Communication/      bluetooth_serial、crsf、slave_link
    Sensors/            mpu6050、mpu6050_regs
  Project.uvprojx        Keil 构建工程
  Project.uvoptx         与构建工程一致的文件分组和调试选项
Slave_MCU/
  Core/                 main.c、STM32 中断支持和 SPL 配置
  App/                  slave_app、slave_scheduler
  BSP/                  slave_board、board_inputs
  Drivers/
    Bus/                soft_spi（保留的历史软件 SPI）
    Communication/      master_link
    Display/            oled、oled_font、at7456e
    Sensors/            optical_flow、qmc5883p
      BMP390/           bmp390.c/h、bmp3.c/h、bmp3_defs.h
  Project.uvprojx
  Project.uvoptx
Shared/
  Protocol/             inter_mcu_protocol：固定帧编码、解码与 CRC
  Scheduling/           periodic_tasks：纯 C 周期任务合并
  Drivers/              dma_rx、millisecond_clock：两端共用的 STM32 辅助驱动
Platform/STM32F1/
  CMSIS/                启动汇编、内核与系统时钟支持
  SPL/                  ST 标准外设库
  System/               公共 Delay 服务
```

`tests/host` 保存 8 组主机测试及寄存器桩，`tools` 保存路径检查和双目标 Keil 构建脚本，`docs` 保存学习与维护文档。调试配置文件和清理脚本继续留在各 MCU 工程目录。

## 职责与依赖

| 层 | 职责与约束 |
|---|---|
| Core | 启动、主循环入口和 STM32 支持文件。本次保留主控 main.c 中的业务编排及 TIM2 ISR，不拆出新的应用文件。 |
| App | 周期任务、安全策略、传感器采集与数据发布流程；调用控制、驱动和 BSP 接口。 |
| BSP | 当前板卡的资源和接线：时基、PWM、LED、看门狗、外部输入、舵机、电池 ADC、蜂鸣器。 |
| Control | 姿态估计、滤波与 PID 计算。pid_controller 是纯 C；attitude_estimator 仍读取 MPU6050，本次不改变其调用关系。 |
| Drivers | 总线操作、串口协议接入、设备寄存器和显示。现有驱动内的 GPIO/总线初始化保留，不为分层拆出重复适配文件。 |
| Shared | 共享协议、调度和硬件辅助实现只保留一份；Protocol/Scheduling 不访问寄存器，Drivers 可依赖 STM32 平台。 |
| Platform | CMSIS、SPL 和公共延时；不依赖飞控业务。不在主从目录复制平台库。 |

`slave_link` 位于主控，表示通往从控的接收链路；`master_link` 位于从控，表示通往主控的发送链路。两者共用 `Shared/Protocol/inter_mcu_protocol`，传输方向和协议未改变。

`Drivers/Sensors/BMP390/bmp390.c` 是项目自有的 SPI 适配层；同目录中的 `bmp3.c/h` 和 `bmp3_defs.h` 是 Bosch 原厂代码，保持原内容、API 和版权声明。这里不另设 ThirdParty 目录。

## 命名规则

- 项目自有 C 文件、函数和变量采用 `lower_snake_case`，公共函数带模块前缀，类型使用 `_t` 后缀。
- 宏和枚举项使用 `UPPER_SNAKE_CASE`，头文件保护宏带工程/路径前缀，避免保留的双下划线名称。
- 设置用 `set`，读取用 `get`；更新滤波或控制状态用 `update`。单位明确时使用 `_ms`、`_cm`、`_mm`、`_deg`、`_dps` 等后缀。
- `main`、中断向量函数、CMSIS/SPL/Bosch API 及平台文件名按外部约定保留。
- 串口命令字符串属于外部协议，例如 `PKp`、`Contrl_Speed`，保留其原拼写；相关 C 变量已改为英文语义名称。

| 原名称 | 当前名称/语义 |
|---|---|
| hubu.c / CompFilter_Simple | Control/attitude_estimator.c / attitude_estimator_update |
| MyI2C.c / MyI2C_* | Drivers/Bus/soft_i2c.c / soft_i2c_* |
| MySPI.c / MySPI_* | Drivers/Bus/soft_spi.c / soft_spi_* |
| Pid.c / Drone_Inner_Rate_PID_Control | Control/pid_controller.c / pid_update_rate_loop |
| Pitch_Kp_Get（实际设置） | pid_set_pitch_kp |
| Pitch_Back_Kp（读蓝牙参数） | bluetooth_serial_get_pitch_kp |
| Get_Motor_Duty_FrontLeft（返回 CCR 计数） | pid_get_motor_compare_front_left |
| Kalman_Get_Roll（实际更新滤波器） | kalman_update_roll |
| PWM4.c | BSP/motor_pwm.c；仍使用 TIM4、原通道映射和预装载配置 |
| BlueSerial.c / SlaveMCU.c | Drivers/Communication/bluetooth_serial.c / slave_link.c |
| 从控 slave_link.c / exti.c | Drivers/Communication/master_link.c / BSP/board_inputs.c |
| yaw_ceshi / Contrl / MAG_intf | yaw_output_snapshot / control_throttle_percent / mag_calibration_switch |

## 本次重构边界

仅迁移和重命名、调整引用与工程分组。算法表达式、数据类型、常量、控制周期、DMA 大小、协议内容、外设寄存器配置、中断入口和处理顺序均保留。未删除保留的历史驱动，未扩大 main.c 拆分范围。

`soft_spi` 是现有历史软件 SPI，不替代 BMP390 当前使用的硬件 SPI1。启用历史驱动前仍需检查引脚资源。

运行频率和事件处理方式见 [RUNTIME_SCHEDULING.md](RUNTIME_SCHEDULING.md)，测试与实机验证边界见 [TESTING.md](TESTING.md)。
