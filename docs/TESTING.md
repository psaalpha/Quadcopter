# 自动测试与验证说明

## 测试目标

本项目采用分层验证：

```text
静态结构校验
    ↓
电脑端纯 C 单元测试
    ↓
Master/Slave ARMCC 构建
    ↓
无桨台架测试
    ↓
系留与受控飞行测试
```

上层测试不能替代下层硬件验证，但能够更早发现结构和逻辑回归。

## 结构校验

```powershell
python tools/validate_project.py
```

当前检查：

- 必需工程文件存在；
- Master/Slave 不重新出现重复平台目录；
- 两个 Keil XML 可以解析；
- Keil 引用的源文件和 include 目录存在；
- Master/Slave 都编译共享协议；
- 核心维护文档存在。

## Host 单元测试

```powershell
cmake -S . -B build/host -G "MinGW Makefiles"
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

测试替身、故障注入约束和新增测试步骤见
[`tests/host/README.md`](../tests/host/README.md)。

当前测试矩阵：

| 测试 | 主要范围 |
|---|---|
| `inter_mcu_protocol` | 帧格式、端序、CRC 和损坏拒绝 |
| `flight_safety` | 启动锁、失联、恢复和 tick 回绕 |
| `driver_status` | 生命周期记录、连续错误和饱和计数 |
| `status_led` | GPIO HAL、active-low 和 I/O 故障恢复 |
| `app_task_model` | 周期、优先级、deadline 和栈预算 |
| `parameter_store` | 类型、范围、schema、CRC 和事务加载 |
| `event_log` | FIFO、回绕、溢出、锁和 sink backpressure |

所有 Host 测试在 Debug 和 Release 配置中都显式保留 `assert()`；禁止通过
`NDEBUG` 把断言编译掉。

## 统一质量门禁

Windows 开发环境可一次运行结构校验、Host 测试和双 Keil 构建：

```powershell
.\tools\run_quality_gates.ps1
```

未安装 Keil、只运行可移植检查时：

```powershell
.\tools\run_quality_gates.ps1 -SkipFirmware
```

脚本在 Windows 检测到 `mingw32-make` 时会自动选择 MinGW，也可以使用
`-Generator` 显式指定已安装的 CMake 生成器。

该脚本不清理构建目录，也不烧录硬件。

### 协议测试

覆盖：

- CRC16-CCITT 标准向量；
- 小端序；
- 有符号数；
- 编码/解码往返；
- 错误长度；
- magic 错误；
- payload 损坏后的 CRC 拒绝。

### 安全状态测试

覆盖：

- 上电锁；
- 高油门不能解锁；
- 低油门进入 ACTIVE；
- 300 ms 失联；
- 高油门恢复锁；
- 低油门恢复；
- 32 位 tick 回绕。

## ARMCC 固件构建

```powershell
.\tools\build_firmware.ps1
```

验收条件：

- Master：`0 Error(s), 0 Warning(s)`；
- Slave：`0 Error(s), 0 Warning(s)`；
- 构建日志保存于各目标 `Objects/engineering_build.log`；
- 协议变化时两个目标必须同时构建。

## 无桨台架测试

至少验证：

| 场景 | 预期 |
|---|---|
| 上电高油门 | 四路保持最小输出 |
| 低油门建立链路 | 允许进入 ACTIVE |
| 拔掉接收机 | 300 ms 内进入 LINK_LOSS |
| 高油门重连 | 保持 RECOVERY_LOCK |
| 主从断线 | Master 不发布损坏数据 |
| 注入错误 CRSF CRC | 不刷新 RC 链路 |
| 注入错误主从 CRC | CRC 计数增加，数据不更新 |
| 主循环人为延迟 | scheduler overrun 可观察 |

## 何时必须增加测试

- 修复曾经出现的缺陷；
- 修改协议字段；
- 修改状态机；
- 修改时间回绕或超时逻辑；
- 新增参数范围；
- 把硬件逻辑提取为纯 C 模块；
- 修改任何电机放行条件。

测试命名应描述行为，而不是函数实现。
