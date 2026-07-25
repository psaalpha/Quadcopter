# 开发维护指南

## 开始工作前

```powershell
git switch <feature-branch>
git pull --ff-only
git status --short --branch
python tools/validate_project.py
```

功能开发从稳定 `develop` 派生；当前长期工程化方向使用
`embedded-engineering-upgrade`。确认工作树干净，并记录当前 Master/Slave 构建结果。

## 修改分类

### 文档或工程配置

示例：README、Keil 路径、CI、构建脚本。

最低验证：

- `python tools/validate_project.py`
- 相关命令实际执行一次

### 驱动修改

最低验证：

- 工程结构校验；
- 受影响目标 ARMCC 0 error/0 warning；
- 总线、引脚和 DMA 资源复核；
- 设备断开、超时或错误响应测试。

### 协议修改

最低验证：

- 更新 `PROTOCOL.md`；
- 增加 host test；
- Master/Slave 同时构建；
- 标明是否破坏兼容；
- 破坏性变化增加协议版本并要求成对烧录。

地面站协议和主从 MCU 协议独立版本化，不得复用消息编号或在未记录的情况下改变语义。

### 安全或控制修改

最低验证：

- 描述故障模式；
- 增加纯逻辑测试；
- Master/Slave 双目标构建；
- 无桨台架验证；
- 不与大规模目录移动放在同一提交。

## 新增驱动步骤

1. 在 `PINOUT.md` 中预分配资源；
2. 检查 GPIO、TIM、DMA、USART、SPI、I2C 和 IRQ 冲突；
3. 确定驱动属于 BSP、总线层还是设备层；
4. 按 `DRIVER_API.md` 定义公开接口；
5. 把寄存器操作限制在驱动内部；
6. 明确阻塞时间、超时和 ISR 上下文；
7. 添加到对应 Keil 工程；
8. 运行结构校验和目标构建；
9. 更新 README 或模块文档；
10. 单独提交，提交信息说明目的。

## 新增周期任务步骤

1. 说明任务周期和最坏执行时间；
2. 判断是否真的需要新定时器；
3. 优先复用调度器，不在 ISR 中实现业务；
4. 定义任务过载时采用丢弃、合并还是排队；
5. 增加 overrun 或执行时间诊断；
6. 更新 `ARCHITECTURE.md` 的运行模型。

## 代码审查清单

- 是否新增了公开全局变量？
- 是否在 ISR 中加入浮点、格式化或等待？
- 是否存在无超时的硬件轮询？
- 是否明确了物理单位？
- 是否修改了主从协议布局？
- 是否改变了电机输出或安全状态？
- 是否引入引脚、DMA 或定时器冲突？
- 是否同步更新测试、文档和 CHANGELOG？
- 新 public API 是否有 Doxygen 契约、单位和调用上下文？
- `Shared` 是否仍然不依赖 STM32、动态内存和格式化 stdio？
- 是否保留了 `main` 的稳定基线？

## Git 提交要求

推荐提交结构：

```text
动词开头的简短标题

说明为什么修改、架构影响以及验证方法。
```

一个提交只完成一个目的，例如：

- `Document driver interface conventions`
- `Add slave protocol freshness tests`
- `Move battery ADC setup into slave BSP`

不要把算法调参、驱动重构、目录迁移和格式化混在一个提交中。

阶段性工程化提交正文应记录：

- `Purpose`：解决的工程问题；
- `Knowledge`：涉及的嵌入式知识；
- `Value`：对可维护性、可靠性或验证的价值。

编码规则见 [C 编码规范](CODING_STANDARD.md)，静态检查路线见
[静态检查说明](STATIC_ANALYSIS.md)。
