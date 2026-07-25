# 静态检查说明

## 1. 当前自动门禁

`python tools/validate_project.py` 是仓库级静态结构检查，当前验证：

- 必需工程和文档文件存在；
- Keil 工程 XML、源文件和 include 引用有效；
- 主从工程都包含共享主从协议；
- Markdown 内部链接有效；
- 每个 `test_*.c` 都注册到 CMake；
- `Shared` 不依赖 STM32 平台 token；
- `Shared` 不使用动态内存和格式化 stdio。

Host 构建对项目自有可移植代码启用高警告等级并将 warning 当作 error。它能发现
未使用变量、隐式声明、类型问题和部分可疑转换，但不能替代面向 Cortex-M 的分析。

## 2. 本地执行

推荐统一执行：

```powershell
.\tools\run_quality_gates.ps1
```

只做可移植检查：

```powershell
.\tools\run_quality_gates.ps1 -SkipFirmware
```

单独执行结构和静态边界检查：

```powershell
python tools/validate_project.py
```

## 3. 后续工具路线

### Stage A：Cppcheck

先对 `Shared`、`Master_MCU/App` 和 `Master_MCU/BSP` 启用，排除 CMSIS/SPL。
建议类别：warning、style、performance、portability。固定工具版本，并把命令写入 CI。

### Stage B：Clang-Tidy / Clang analyzer

使用 Host CMake 生成的 compile database 分析纯 C 模块。重点规则：

- 越界、空指针、未初始化；
- narrowing/sign conversion；
- 可疑分支和不可达代码；
- API ownership 和 lifetime；
- duplicated branches 与 dead store。

ARMCC 扩展、SPL 宏和中断声明需要单独配置，不应靠大量无范围 suppressions 掩盖。

### Stage C：商业/合规工具

产品化时评估 PC-lint Plus、Helix QAC、Coverity 或同等级工具，并建立
MISRA C:2012 deviation record。工具选择应以 MCU 编译器、CI 许可和团队审查能力为准。

## 4. 告警治理

- 新代码不增加告警；
- 告警必须修复、证明误报或形成有 owner/理由/范围的 deviation；
- 禁止全局关闭规则来解决单文件问题；
- baseline 只能隔离遗留债务，不能让新问题进入；
- 安全相关告警由第二人复核；
- 每次工具升级记录新增告警和规则变化；
- 静态分析通过不代表实时性、硬件行为或控制安全已经验证。

## 5. 建议优先规则

1. buffer 长度、数组索引和整数溢出；
2. 空指针、未初始化和 use-after-lifetime；
3. switch 枚举完整性；
4. ISR 与任务共享变量；
5. 无 timeout 的循环；
6. 隐式有符号转换；
7. 浮点异常和除零；
8. 返回值忽略；
9. 不可达代码和重复条件；
10. 动态内存和递归。

## 6. 静态检查之外

以下仍需要动态或硬件验证：

- 500 Hz 控制周期 WCET 和 jitter；
- NVIC 优先级与 ISR nesting；
- DMA/cache/volatile 可见性；
- I2C/SPI 电气错误和总线恢复；
- PWM 边沿和电机安全输出；
- watchdog、掉电和 Flash 原子性；
- 栈深度和 RTOS priority inversion；
- HIL 输入回放与算法输出等价性。
