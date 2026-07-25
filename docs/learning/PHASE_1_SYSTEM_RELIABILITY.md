# Phase 1 学习指南：系统可靠性

## A. 这个模块解决什么工程问题

普通“主循环喂狗”只能发现 CPU 完全卡死。企业项目需要发现局部任务失效、
统一表达不同来源的故障，并保证关键故障不会自动恢复到危险状态。

本阶段建立了：

- 基于 heartbeat 的喂狗许可；
- 统一 Fault ID/Level/Record；
- 可审查的 Failsafe 状态转换。

## B. 设计思想

- 看门狗是最后一道复位机制，不是定时喂狗 API；
- 故障事实与故障动作分离；
- 状态转换必须显式、守卫条件必须可测试；
- Critical/Fatal 故障采用 fail-safe 默认值；
- 历史故障与当前故障分开保存；
- 所有时间判断使用支持回绕的无符号差值。

## C. 涉及嵌入式知识

- 独立看门狗与窗口看门狗；
- STM32 RCC reset flags；
- heartbeat、deadline 与 supervisor；
- 故障分级、故障锁存和降级运行；
- 有限状态机、guard、事件驱动；
- fail-safe 与 fail-operational 的区别；
- tick overflow；
- 故障注入测试。

## D. 面试如何描述

可以这样描述：

> 我把无条件喂狗改造成基于关键任务 heartbeat 的 Watchdog Manager。只有
> Sensor、Control、Safety 等 required client 都在各自 deadline 内完成有效
> 工作时才允许调用硬件 feed。同时用 Fault Manager 统一故障 ID、等级、时间
> 和锁存历史，再通过显式 Failsafe 状态机把 Warning、Critical 和 Disarm
> 行为分开。所有模块先在主机端做超时、tick 回绕和状态转换测试，再计划接入
> 电机许可路径。

重点强调“没有在第一步就替换稳定飞行保护”。

## E. 后续我需要自己学习什么

1. 阅读 STM32F1 RCC CSR 和 IWDG/WWDG 章节；
2. 计算 LSI 误差对 IWDG timeout 的影响；
3. 学习 ISO 26262/IEC 61508 中的 fault reaction 基本思想；
4. 为 IMU、CRSF、主从链路和电池定义明确故障阈值；
5. 在板上实现 reset reason BSP adapter；
6. 设计 retained RAM 或 Backup Register 保存复位前最后故障；
7. 完成无桨故障注入和看门狗复位测试。
