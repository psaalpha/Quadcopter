# Phase 6 学习指南：FreeRTOS 迁移准备

## A. 这个模块解决什么工程问题

把裸机飞控直接拆成多个抢占任务，会改变执行顺序、数据并发、RAM、抖动和故障模式。
本模块先把五个目标任务和所有权写成可测试契约，使迁移从“改完再调”变成分阶段验证。

## B. 设计思想

- 当前协作式模型和未来 RTOS 模型并存，但未来模型不执行；
- Sensor 采样完成后通知 Control，固定数据相位；
- Safety 最高优先级且短小有界，Logger 最低且允许丢记录；
- 多字段数据以不可变快照或队列跨任务；
- 外设采用单一 owner，避免到处加 mutex；
- 静态分配、heartbeat、deadline 和 stack watermark 都是上线条件；
- 慢任务先迁，控制链最后迁，并保留可回退基线。

## C. 涉及嵌入式知识

- RMS/固定优先级调度与 response time；
- 周期、deadline、WCET 和 jitter；
- task notification、queue、mutex、priority inheritance；
- ISR `FromISR` API 和中断优先级；
- 双缓冲、seqlock 和单 writer；
- 静态内存、任务栈和高水位；
- 优先级反转、死锁和 watchdog supervision；
- trace、DWT cycle counter 和 HIL 对照。

## D. 面试如何描述

> 我没有直接把裸机飞控换成 FreeRTOS，而是先建立五任务迁移契约。Sensor 以
> 2 ms 绝对周期采样并通知 Control，Safety 最高优先级只做有界判定，通信和日志
> 不能阻塞控制。跨任务数据使用快照/队列，外设采用单 owner。迁移顺序从日志和通信
> 开始，最后才迁控制链，并用 WCET、jitter、stack watermark、故障注入和输出回放
> 证明与原算法时序等价。

## E. 后续我需要自己学习什么

1. FreeRTOS scheduler、task notification 和 queue internals；
2. Cortex-M NVIC 优先级与 `configMAX_SYSCALL_INTERRUPT_PRIORITY`；
3. `vTaskDelayUntil()`、tick wrap 和 release jitter；
4. response-time analysis 与 CPU utilization bound；
5. Percepio/SystemView/tracealyzer 或 GPIO trace；
6. stack overflow hook、hard fault dump 和 static allocation；
7. HIL 输入回放与双执行模型差分测试；
8. 优先级反转、死锁和 fault injection。
