# 工程文档索引

本目录是 Quadcopter 项目的维护入口。首次接手项目时，建议按下列顺序阅读。

## 推荐学习路径

1. [项目结构](PROJECT_STRUCTURE.md)：先确认目录、模块和代码所有权。
2. [总体架构](ARCHITECTURE.md)：理解四次工程化改造及分层原则。
3. [企业级嵌入式升级计划](EMBEDDED_ENGINEERING_UPGRADE.md)：理解分阶段目标、边界和验收条件。
4. [硬件资源与引脚](PINOUT.md)：理解 MCU 外设和引脚占用。
5. [驱动接口规范](DRIVER_API.md)：理解现有驱动和新驱动的接口要求。
6. [HAL 抽象设计](HAL.md)：理解设备驱动、HAL、BSP 和测试替身的关系。
   完整总线和定时器契约见 [HAL 总体设计](HAL_DESIGN.md)。
7. [主从通信协议](PROTOCOL.md)：理解双 MCU 数据交换。
   面向上位机的稳定接口见 [地面站通信协议](GROUND_STATION_PROTOCOL.md)。
8. [主控安全状态](SAFETY.md)：理解电机放行和失联行为。
9. [系统可靠性设计](SYSTEM_RELIABILITY.md)：理解 Watchdog、Fault 和系统 Failsafe。
10. [FreeRTOS 任务架构](FREERTOS_ARCHITECTURE.md)：理解任务、优先级、栈和迁移门槛。
11. [参数管理系统](PARAMETERS.md)：理解参数校验、版本、CRC 和持久化边界。
12. [结构化日志系统](LOGGING.md)：理解固定记录、缓冲、并发和输出后端。
13. [Flight Data Logger](FLIGHT_DATA_LOGGER.md)：理解飞行快照、带宽和非阻塞输出。
14. [构建与验证](BUILD.md)：建立本地编译环境。
15. [自动测试说明](TESTING.md)：理解质量门槛和测试范围。
16. [开发维护指南](DEVELOPMENT_GUIDE.md)：按标准流程修改项目。
17. [运行维护手册](MAINTENANCE.md)：定位常见构建、通信和硬件问题。
18. [发布流程](RELEASE.md)：生成、验证和烧录匹配固件。
19. [后续路线](ROADMAP.md)：了解当前边界和下一阶段工作。

## 按角色阅读

| 角色 | 优先文档 |
|---|---|
| 新入门 STM32 开发者 | 项目结构、引脚、构建、驱动接口 |
| 飞控算法开发者 | 总体架构、安全状态、测试说明 |
| 底层驱动开发者 | 引脚、驱动接口、开发维护指南 |
| 集成与测试人员 | 构建、测试、安全、发布流程 |
| 项目维护者 | 全部文档、`CHANGELOG.md`、`CONTRIBUTING.md` |

## 文档维护规则

- 行为、协议、引脚、周期或安全条件变化时，必须同步更新对应文档。
- 文档中的路径、命令和接口名称必须与当前 `develop` 分支一致。
- 不确定的信息应标记为“待硬件验证”，不能把推测写成事实。
- 每个工程化提交都应在 `CHANGELOG.md` 记录目的、影响和验证结果。
