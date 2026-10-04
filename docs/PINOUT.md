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
| TIM1 | 不再用于周期调度 | BSP |
| TIM2 | 1ms 统一时基，发布 2/5/10/100ms 任务 | BSP |
| TIM3 | 不再用于周期调度 | BSP |
| TIM4 | 50Hz 四路 ESC PWM；500Hz内环写CCR预装载，不启用 PWM 更新中断 | Motor PWM |
| DMA1_CH3 | USART3 RX | SlaveMCU |
| DMA1_CH4/CH5 | USART1 TX/RX | BlueSerial |
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
| DMA1_CH7 | USART2 主从 TX，41 字节单缓冲 | SlaveLink |

PA2 用于连接主控 USART3 RX；PA3 是蜂鸣器，不配置为串口 RX。硬件接线必须共地。

周期和时钟详见 [运行调度](RUNTIME_SCHEDULING.md)。
