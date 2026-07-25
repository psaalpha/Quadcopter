# Phase 2 学习指南：硬件抽象层

## A. 这个模块解决什么工程问题

设备驱动直接操作 STM32 SPL 后，代码无法主机测试、无法复用到其他 MCU，也
很难注入 BUSY、NACK、DMA 错误等故障。HAL 把设备行为和芯片访问分开。

## B. 设计思想

- 接口契约稳定，平台实现可替换；
- context + const ops 实现 C 语言依赖注入；
- buffer 所有权和阻塞属性必须明确；
- 异步 Start/Poll 代替循环等待；
- 单位进入 API 名称；
- PWM wrapper 在底层写入前校验范围；
- 历史驱动逐个迁移，不做大爆炸重构。

## C. 涉及嵌入式知识

- 函数指针和接口表；
- UART DMA ring；
- I2C repeated-start、NACK、bus recovery；
- SPI mode、chip select 和 full duplex；
- PWM timer tick、ARR/CCR；
- 定时器频率与计数回绕；
- ISR/任务 buffer 所有权；
- mock/fake 与依赖注入。

## D. 面试如何描述

> 我为 UART、I2C、SPI、PWM 和 TIMER 建立了无芯片依赖的 HAL contract，
> 使用 context + const function table 做静态依赖注入。通信总线采用非阻塞
> start/poll/cancel，PWM 在调用平台 adapter 前校验 tick 范围。设备驱动只
> 看 HAL，STM32 SPL、DMA、NVIC 和引脚映射只存在于 BSP adapter。为了控制
> 风险，我先迁移 LED，再按设备逐个迁移通信和传感器。

## E. 后续我需要自己学习什么

1. STM32 USART + DMA circular receive；
2. I2C 总线挂死与 9-clock recovery；
3. SPI DMA、片选时序和 mode 切换代价；
4. TIM preload、update event 和 CCR 生效时点；
5. ISR-safe ring buffer；
6. 用逻辑分析仪验证 UART/SPI/I2C；
7. 用示波器对比 PWM 迁移前后周期和脉宽。
