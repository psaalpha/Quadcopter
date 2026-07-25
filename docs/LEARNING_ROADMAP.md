# 企业级嵌入式飞控学习路线

这份路线把本次七个 Phase 变成可逐项深入的学习顺序。基础实现是边界和练习载体，
不是已经完成飞行验证的产品功能。

## 总览

| Phase | 工程主题 | 当前基础成果 | 学习指南 |
|---|---|---|---|
| 1 | 系统可靠性 | Watchdog、Fault、Failsafe | [Phase 1](learning/PHASE_1_SYSTEM_RELIABILITY.md) |
| 2 | 硬件抽象 | GPIO/UART/I2C/SPI/PWM/TIMER HAL | [Phase 2](learning/PHASE_2_HAL_ARCHITECTURE.md) |
| 3 | 飞行数据 | 非阻塞 Flight Data Logger | [Phase 3](learning/PHASE_3_FLIGHT_DATA_LOGGER.md) |
| 4 | 参数调试 | Catalog、校验、schema、持久化后端 | [Phase 4](learning/PHASE_4_PARAMETER_DEBUG_SYSTEM.md) |
| 5 | 地面站接口 | 版本帧、CRC、参数和控制请求 | [Phase 5](learning/PHASE_5_GROUND_STATION_INTERFACE.md) |
| 6 | RTOS 准备 | 五任务契约、数据流和迁移门禁 | [Phase 6](learning/PHASE_6_FREERTOS_MIGRATION.md) |
| 7 | 工程质量 | Doxygen、规范、静态分析、质量门禁 | [Phase 7](learning/PHASE_7_ENGINEERING_QUALITY.md) |

## 建议学习顺序

### 第一阶段：先能解释现有系统

1. 画出 Master/Slave 数据流和硬件资源图；
2. 从 `main` 跟踪一次 Sensor → PID → Mixer → PWM 调用；
3. 读懂主从协议和 CRSF 失联安全状态；
4. 在无桨条件测量 2 ms/5 ms/10 ms/20 ms 实际周期；
5. 学会运行 15 个 Host 测试和双固件构建。

输出物：一张数据流图、一张定时图、一份 RAM/Flash map 注释。

### 第二阶段：掌握可测试边界

1. 为一个简单设备实现 HAL backend + fake；
2. 给 I2C/SPI 增加 timeout 与故障注入；
3. 练习快照、ring buffer 和 callback/context；
4. 给参数/协议增加一个兼容字段和测试；
5. 用 CRC 损坏、队列满和 tick wrap 写负向测试。

输出物：一个不需要板卡即可测试的新驱动。

### 第三阶段：掌握可靠性

1. 把 Watchdog backend 绑定到 STM32 reset flags/IWDG；
2. 定义哪些任务是 required heartbeat；
3. 建立 Fault → Level → Failsafe action 表；
4. 设计 DISARM 后才允许的 Flash 行为；
5. 做 IMU 断开、串口失联、低电压和任务卡死故障注入。

输出物：可重复的无桨故障测试报告。

### 第四阶段：掌握实时系统

1. 使用 DWT/GPIO trace 测 WCET 和 jitter；
2. 完成五任务 response-time 和 RAM 预算；
3. 学习 NVIC 与 FreeRTOS syscall interrupt priority；
4. 先迁 Logger/Communication，再迁 Safety；
5. 最后对 Sensor/Control 做输入回放差分。

输出物：迁移前后 timing、stack、output 差分报告。

### 第五阶段：产品化

1. Flash 双槽、掉电恢复和 schema migration；
2. 地面站 UART DMA 流解析、认证与限流；
3. flight log 后端和离线解析工具；
4. 静态分析 baseline、MISRA deviation 和 review；
5. HIL、24h soak、版本发布和回滚演练。

输出物：可审计的 release evidence package。

## 每个模块的学习方法

对每个 Phase 重复五步：

1. **读契约**：先看 `.h` 和对应设计文档；
2. **画状态/数据流**：不看实现复述边界；
3. **跑测试**：逐个破坏输入理解负向路径；
4. **做最小硬件绑定**：只接一个 backend，不一次全接；
5. **写验证证据**：测量数据、故障结果和仍未覆盖的风险。

## 面试项目叙述主线

> 我把一个能运行的双 STM32 四旋翼项目逐步工程化。先分离平台与业务，再建立
> HAL、参数、日志、协议和主机测试；可靠性层使用 heartbeat gating、统一 Fault 和
> 显式 Failsafe；RTOS 没有直接切换，而是先做任务所有权、WCET、RAM 和数据流设计，
> 通过慢任务优先迁移、HIL 差分和故障注入保护原 PID 与 PWM 时序。每一步都独立提交，
> 有文档、测试和双目标零告警构建证据。

面试时要明确区分“已实现基础模块”“已接入生产路径”“已通过硬件验证”三种状态，
不要把接口预留描述成已飞行验证。
