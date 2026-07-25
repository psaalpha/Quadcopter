# 硬件资源与引脚

本表以当前代码实际初始化路径为准。修改引脚时，应同时更新 `BSP`、驱动、Keil 工程和本文档。

## Master MCU

| 引脚/资源 | 功能 | 归属 |
|---|---|---|
| PA2 / PA3 | USART2 TX/RX，CRSF 420000 | CRSF |
| PA6 / PA7 | 软件 I2C，MPU6050 | IMU |
| PA9 / PA10 | USART1 TX/RX，蓝牙调参 38400 | BlueSerial |
| PB6 / TIM4_CH1 | 后左电机 | Motor PWM |
| PB7 / TIM4_CH2 | 前右电机 | Motor PWM |
| PB8 / TIM4_CH3 | 前左电机 | Motor PWM |
| PB9 / TIM4_CH4 | 后右电机 | Motor PWM |
| PB10 / PB11 | USART3 TX/RX，主从链路 115200 | SlaveMCU |
| PC13 | LED1 | 状态指示 |
| PA0 | LED2 | CH4 指示 |
| PA5 | LED3 | CH5 指示 |
| TIM1 | 5 ms RC 服务时基 | BSP |
| TIM2 | 2 ms IMU/内环时基 | BSP |
| TIM3 | 10 ms角度外环时基 | BSP |
| TIM4 | 20 ms 四路 ESC PWM | Motor PWM |
| DMA1_CH3 | USART3 RX | SlaveMCU |
| DMA1_CH4/CH5 | USART1 TX/RX | BlueSerial |
| DMA1_CH6 | USART2 RX | CRSF |

`NRF24L01`、主控 OLED 和其他遗留驱动仍保留在工程中，但当前 `main` 没有初始化。它们与 PA0/PA5/PA6 或 PB12/PB13 等资源存在潜在复用冲突，启用前必须先完成资源审查。

## Slave MCU

| 引脚/资源 | 功能 | 归属 |
|---|---|---|
| PA0 | 外部按键/归零 EXTI | exti |
| PA1 | 舵机档位输入 | User |
| PA2 | USART2 TX，发送给 Master | Inter-MCU |
| PA3 | 低电平有效蜂鸣器 | Battery alarm |
| PA4–PA7 | SPI1，BMP390 | Barometer |
| PA8 | 磁力计校准触发 | User |
| PA9 / PA10 | USART1 TX/RX，光流 115200 | OpticalFlow |
| PB0 / TIM3_CH3 | 舵机 PWM | Servo |
| PB1 / ADC1_IN9 | 电池电压 | Battery |
| PB8 / PB9 | 软件 I2C，OLED | Display |
| PB10 / PB11 | 软件 I2C，QMC5883P | Magnetometer |
| PB12–PB15 | SPI2，AT7456E OSD | OSD |
| TIM2 | 50 ms 传感器任务时基 | User |
| TIM3 | 20 ms 舵机 PWM | Servo |

从控注释中的 PA2 “上位机”实际用于连接主控 USART3 RX；硬件接线必须共地。
