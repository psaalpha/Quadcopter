# C 编码规范

## 1. 适用范围

本规范适用于项目自有 C 源码。`Platform/STM32F1/CMSIS` 和 `SPL` 是第三方代码，
不要求追溯格式，但对其修改必须独立审查。现有遗留代码按触碰式治理，不做无目的全仓格式化。

项目同时面向 ARMCC 5 和电脑端 GCC/Clang，因此公共模块应使用两者都能稳定接受的 C 子集。

## 2. 依赖和分层

- Application 只表达业务编排和策略；
- Middleware/Services 不访问 STM32 寄存器；
- `Shared/HAL` 只定义硬件无关契约；
- BSP/Platform 负责将 HAL 绑定到 STM32 SPL；
- `Shared` 禁止包含 `stm32f10x*` 或具体板级头文件；
- PID、姿态、mixer 不反向依赖日志、通信或参数持久化；
- 新模块必须有唯一 owner，禁止用公开可写全局变量共享状态。

## 3. 命名

| 对象 | 规则 | 示例 |
|---|---|---|
| 公共函数 | `Module_VerbNoun` | `FaultManager_Raise` |
| 公共类型 | PascalCase | `GroundStationFrame` |
| 枚举常量 | `MODULE_GROUP_VALUE` | `FAULT_LEVEL_CRITICAL` |
| 宏 | 全大写 + 模块前缀 | `GROUND_STATION_MAX_PAYLOAD_SIZE` |
| 文件私有函数 | `Module_VerbNoun` + `static` | `GroundStationService_Error` |
| 物理量 | 名称带单位 | `timeout_ms`, `battery_mv` |

不得新增语义含糊的 `data`、`value1`、`flag2` 公共字段。协议字段和参数 ID 一经发布，
不能仅为“更好看”而重新编号。

## 4. 类型和数值

- 跨模块和线上数据使用 `<stdint.h>` 固定宽度类型；
- 布尔状态在现有 ARMCC 基线上使用 `uint8_t` 0/1；
- 长度、容量和索引先验证再转换；
- 时间差使用无符号减法以支持 tick 自然回绕；
- 有符号/无符号转换必须显式；
- 浮点只用于算法和明确允许的快照，不在 ISR 中新增；
- 位掩码使用无符号常量，如 `1u << bit`；
- 饱和计数器不得静默回绕；
- 比较物理量时写清单位和缩放。

## 5. 接口与错误处理

- 公开头文件必须有 include guard、`@file` 和 `@brief`；
- 输入指针和长度在模块边界校验；
- 可能失败的操作返回模块状态枚举，不返回来源不明的 magic number；
- timeout、busy、not found、rejected 与 internal error 应可区分；
- 输出参数只在成功或文档明确规定时更新；
- 初始化必须可判断是否成功；重复初始化语义要明确；
- callback 后端通过 context 注入，不依赖隐藏单例；
- 不在库模块中调用 `printf`、`malloc` 或 `free`。

## 6. 实时和并发

- ISR 只清标志、搬运最小数据、记录时间和通知任务；
- ISR 禁止阻塞、字符串格式化、Flash 写入、协议整帧解析和业务状态机；
- 控制路径禁止等待日志、遥测和参数保存；
- 多字段跨上下文数据用快照、双缓冲或队列；
- 临界区必须短、可测量，不能包围外设等待；
- 锁顺序必须写入模块文档；优先使用单 owner 消除多锁；
- ring/queue 满时必须有明确 drop/backpressure 策略和计数；
- 喂狗只代表所有 required client 已完成有效迭代。

## 7. 内存

- 飞行固件默认禁止业务层动态分配；
- buffer 容量在编译期或初始化时固定；
- 大对象和任务栈必须进入 RAM budget；
- 复制 payload 前验证源长度和目标容量；
- Flash image 必须含 schema、长度和 CRC；
- 敏感持久化使用双槽/原子提交，由后端保证掉电一致性。

## 8. 注释和 Doxygen

注释解释“为什么、约束、单位和故障行为”，不重复代码字面含义。公共接口至少说明：

- 模块职责和不负责的边界；
- 输入单位、合法范围和调用上下文；
- 是否阻塞、是否 ISR-safe；
- 返回状态和输出有效性；
- ownership、lifetime 和并发要求。

Doxygen 生成方法见 [Doxygen 文档](DOXYGEN.md)。

## 9. 提交前检查

1. `git diff --check`；
2. 运行统一质量门禁；
3. 确认没有把 STM32 依赖带入 `Shared`；
4. 检查新增 public API 的注释、测试和文档；
5. 对周期/ISR/控制修改记录实测证据；
6. 对协议变化说明兼容性；
7. 对安全变化增加状态转换或故障用例。
