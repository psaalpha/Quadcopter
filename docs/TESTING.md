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

### estimated_yaw_deg 与导航 PID 测试

- `yaw_estimator_test`：首帧磁力计对齐、0/360° 环绕方向、无效航向拒绝。
- `pid_control_test`：estimated_yaw_deg 最短角误差及混控输出、高度/光流修正输出、退出辅助模式后的积分和输出清零。
- 这些是算法层电脑端测试，不验证传感器装配、串口电气和真实电机转向。

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
| CH6 关闭或光流质量/测距无效 | 高度/光流辅助退出，保留手动目标 |
| 从控磁力计无新样本 | 不重复融合旧航向 |

## 何时必须增加测试

- 修复曾经出现的缺陷；
- 修改协议字段；
- 修改状态机；
- 修改时间回绕或超时逻辑；
- 新增参数范围；
- 把硬件逻辑提取为纯 C 模块；
- 修改任何电机放行条件。

测试命名应描述行为，而不是函数实现。

## 主控串口与 PWM 回归（Linux 主机）

`master_io_test` 编译实际 bluetooth_serial、slave_link、dma_rx 和 motor_pwm 驱动，使用最小寄存器模型验证：中断不解析、连续蓝牙命令、跨 IDLE 半帧、无 IDLE 的 DMA 通知、坏 CRC 重同步、跨缓冲边界、整圈溢出恢复、待处理 TC、32 位计数回绕、遥测忙时不阻塞，以及 PWM 预装载/安全最小输出。模型不证明真实寄存器时序、NVIC 延迟或实机飞行表现，也不替代 Keil 双目标构建。

## 毫秒调度与从机回归

Linux 新增 `scheduler_test`、`slave_io_test`、`slave_app_test`，合计 8 项。
分别验证周期/合并/时间回绕及 300ms 保护，从机 DMA 所有权/分帧/溢出/输入脉冲，
以及 20Hz 发布、10 秒校准等待 TX 后保存、逐字符显示期间持续服务光流。
寄存器模型不证明真实 UART IDLE 清除时序、GPIO 脉冲最短宽度或总线电气特性。
本次云端只完成主机回归、真实 SPL 头文件语法检查和工程引用检查，未完成 ARMCC 链接。
校准 Flash 页保留、体积、ADC 电压、舵机波形和 DMA 通知时序需在 Keil/板上核验。


## 2026-10-08 目录与命名重构验证

基线：`codex/resume-study-spl` 的 `02a54ef`。本次验证覆盖目录迁移、API/变量重命名和工程引用，不代表实机飞行验证。

| 检查 | 结果 |
|---|---|
| 重构前、后全部 8 组主机测试 | 均通过；按 tests/host/CMakeLists.txt 的源文件与 include 集合直接使用 GCC 编译运行 |
| 主机测试源码 GCC 静态分析 | `-fanalyzer -Wshadow -Wall -Wextra -Werror -pedantic` 通过 |
| Keil 工程所列 C 源文件语法检查 | 主控 46、从控 44 个；使用真实 CMSIS/SPL 头文件及 STM32F10X_MD、USE_STDPERIPH_DRIVER 宏，通过 |
| 项目头文件独立包含 | 31 个通过；软件总线、MPU6050、OLED 头文件补齐 stdint.h |
| 源码差异核对 | 136 个 C/头文件仅有约定的标识符、include 和空白/注释变化；表达式、字面值、声明类型和执行顺序保持 |
| Keil 工程配置核对 | 编译源文件集合、逐文件选项、目标设置保持；uvoptx 文件列表与 uvprojx 一致 |
| 结构、CMake 路径、文档链接 | tools/validate_project.py 通过；git diff --check 通过 |
| 平台/Bosch 内容 | 64 个文件保持；Bosch 三个原厂文件只迁移目录，保留 Git blob 和版权 |

环境未提供 Keil/ARMCC、GNU Arm 工具链或 CMake/CTest，因此未验证 CMake 配置生成和 ARM 固件编译、链接、Flash/RAM 占用及板上时序。主机测试通过不能替代 Keil 双目标构建。

从控 GCC 语法检查保留两条基线已有警告：OLED 字库初始化括号，以及 Bosch 源码未使用的 `intf_ptr` 参数；本次未修改这些内容。

在带 CMake 的环境运行本页的 CMake/CTest 命令，在装有 Keil 的 Windows 电脑运行 `tools/build_firmware.ps1`，完成目标工具链复核。
