# Quadcopter STM32 Flight Controller

这是工程 API 文档的 Doxygen 入口。

## 架构入口

- [项目结构与模块职责](PROJECT_STRUCTURE.md)
- [总体架构](ARCHITECTURE.md)
- [HAL 设计](HAL_DESIGN.md)
- [系统可靠性](SYSTEM_RELIABILITY.md)
- [地面站通信协议](GROUND_STATION_PROTOCOL.md)
- [FreeRTOS 迁移计划](FREERTOS_MIGRATION_PLAN.md)
- [七阶段学习路线](LEARNING_ROADMAP.md)

## 当前产品边界

本工程化分支增加可测试的可靠性、HAL、参数、日志、协议和 RTOS 迁移基础，
但没有改变 PID、姿态解算、电机混控、PWM 时序和现有主从通信兼容性。
新模块在完成板级适配、无桨台架、HIL 和故障注入前，不应被描述为已完成飞行验证。
