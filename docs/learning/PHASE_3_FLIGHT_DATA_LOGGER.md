# Phase 3 学习指南：Flight Data Logger

## A. 这个模块解决什么工程问题

飞控问题往往只在飞行中出现。没有同步的姿态、PID、电机、供电和故障数据，
只能猜测原因。直接 `printf` 又会破坏实时性。

## B. 设计思想

- 周期数据和事件日志分开；
- 固定记录、静态内存、无格式化；
- 控制路径只做有上界的复制；
- 使用分频控制带宽；
- 满队列丢新并计数；
- 后台任务有界 drain；
- schema version 保证工具兼容；
- 先建立基础设施，再测量后接入。

## C. 涉及嵌入式知识

- ring buffer；
- producer/consumer；
- 数据快照一致性；
- WCET 与实时预算；
- DMA 异步输出；
- 二进制日志 schema；
- 带宽/RAM 估算；
- backpressure 和 overflow policy。

## D. 面试如何描述

> 我设计了固定 60 字节的 Flight Data Record，覆盖姿态、角速度、PID 输出、
> 四路电机、电池、系统状态和 Fault mask。生产者按分频只做结构复制，静态
> ring 满时丢新并统计，Logger Task 每次只 drain 指定数量，sink busy 时不
> 移除队首。这样日志后端速度不会反向阻塞控制周期。

## E. 后续我需要自己学习什么

1. SPSC/MPSC ring 的并发条件；
2. DMA scatter/gather 或双缓冲；
3. 记录 framing、页 CRC 和断电恢复；
4. SD 卡/外部 Flash 的最坏写入延迟；
5. Python/Linux 端二进制解析；
6. 飞行数据时间同步；
7. 使用记录回放定位 PID 饱和、振荡和供电问题。
