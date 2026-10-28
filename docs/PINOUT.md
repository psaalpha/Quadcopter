# 硬件资源与引脚

本表以当前代码实际初始化路径为准。修改引脚时，应同时更新 `BSP`、驱动、Keil 工程和本文档。

## Master MCU

| 引脚/资源 | 功能 | 归属 |
|---|---|---|
| PA2 / PA3 | USART2 TX/RX，CRSF 420000 | CRSF |
| PA6 / PA7 | 软件 I2C，MPU6050 | IMU |
| PA9 / PA10 | USART1 TX/RX，蓝牙调参 38400 | bluetooth_serial |
| PB6 / TIM4_CH1 | 后左电机 | Motor PWM |
| PB7 / TIM4_CH2 | 前右电机 | Motor PWM |
| PB8 / TIM4_CH3 | 前左电机 | Motor PWM |
| PB9 / TIM4_CH4 | 后右电机 | Motor PWM |
| PB10 / PB11 | USART3 TX/RX，主从链路 115200 | slave_link |
| PC13 | LED1 | 状态指示 |
| PA0 | LED2 | CH5 舵机电平，接 Slave PA1 |
| PA5 | LED3 | CH6 校准脉冲，接 Slave PA8 |
| TIM1 | 不再用于周期调度 | BSP |
| TIM2 | 1ms 统一时基，发布 2/5/10/100ms 任务 | BSP |
| TIM3 | 不再用于周期调度 | BSP |
| TIM4 | 50Hz 四路 ESC PWM；500Hz内环写CCR预装载，不启用 PWM 更新中断 | Motor PWM |
| DMA1_CH3 | USART3 RX | slave_link |
| DMA1_CH4/CH5 | USART1 TX/RX | bluetooth_serial |
| DMA1_CH6 | USART2 RX | CRSF |

主控未启用的 NRF24L01、本地 QMC5883P、OLED、PWM2 和重复 DMA_Serial 驱动已移除；从控对应驱动不变。

## Slave MCU

| 引脚/资源 | 功能 | 归属 |
|---|---|---|
| PA0 | 外部按键/归零 EXTI | exti |
| PA1 | EXTI1 双边沿舵机档位通知 | exti |
| PA2 | USART2 TX，发送给 Master | Inter-MCU |
| PA3 | 低电平有效蜂鸣器 | Battery alarm |
| PA4–PA7 | SPI1，BMP390 | Barometer |
| PA8 | EXTI8 双边沿，记录校准脉冲 | exti |
| PA9 / PA10 | 保留 TX 引脚映射；USART1 RX 光流 115200，DMA/IDLE/HT/TC | OpticalFlow |
| PB0 / TIM3_CH3 | 舵机 PWM | Servo |
| PB1 / ADC1_IN9 | 电池电压 | Battery |
| PB8 / PB9 | 软件 I2C，OLED | Display |
| PB10 / PB11 | 软件 I2C，QMC5883P | Magnetometer |
| PB12–PB15 | SPI2，AT7456E OSD | OSD |
| TIM2 | 1ms 时基，发布 50ms 传感器与 200ms 显示任务 | BSP |
| TIM3 | 20 ms 舵机 PWM，预装载 | Servo |
| DMA1_CH5 | USART1 光流 RX，256 字节循环缓冲 | OpticalFlow |
| DMA1_CH7 | USART2 主从 TX，53 字节单缓冲 | SlaveLink |

PA2 用于连接主控 USART3 RX；PA3 是蜂鸣器，不配置为串口 RX。硬件接线必须共地。

周期和时钟详见 [架构](ARCHITECTURE.md)。


## 遥控通道（物理 CH 编号）

CH1 横滚、CH2 俯仰、CH3 油门、CH4 偏航、CH5 舵机、CH6 磁校准、CH7 高度/位置辅助、CH8 ARM。
软件数组下标比物理通道号少 1；CH8 OFF <1300 / ON >1700。
主控 PA0/PA5 为低电平有效，Slave 舵机输入高=2000µs、低=1000µs；CH5 高档使 PA0 拉低。
Slave PA0 按键低电平归零气压高度，20ms 消抖；PA8 需完整 1→0→1 校准脉冲。

CV-0850 TX 接 Slave PA10，115200 8N1；原生或 MSP/INAV 输出，轴向按实际安装验证。
ADC PB1 的现有换算为 3.3V×4 分压，默认低压阈值按 3S 电池配置。
两块板各自的 0x0800F800 和 0x0800FC00 为配置页，不可用于应用程序。
