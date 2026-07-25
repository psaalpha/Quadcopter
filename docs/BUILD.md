# 构建与验证

## 固件工具链

当前 Keil 工程使用：

- Keil µVision 5
- ARMCC 5.06 update 7
- STM32F10x Standard Peripheral Library
- 目标器件 STM32F103C8

打开 `Master_MCU/Project.uvprojx` 或 `Slave_MCU/Project.uvprojx` 可以单独构建。推荐从仓库根目录执行统一脚本：

```powershell
.\tools\build_firmware.ps1
```

脚本按顺序构建 Master 和 Slave，只有两个目标都报告 `0 Error(s), 0 Warning(s)` 才成功。构建日志写入各自 `Objects/engineering_build.log`，该目录不会提交到 Git。

## 电脑端测试

依赖：

- CMake 3.16 或更高
- GCC、Clang 或 MSVC
- Python 3.9 或更高

执行：

```powershell
python tools/validate_project.py
cmake -S . -B build/host -G "MinGW Makefiles"
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

当前测试覆盖：

- 主从与地面站协议、CRC、字节序和错误拒绝；
- 飞行安全、Watchdog、Fault 和 Failsafe；
- HAL 后端契约、驱动状态和 LED fake；
- 参数、持久化、结构化事件和 Flight Data Logger；
- 当前协作任务模型与未来五任务 FreeRTOS 计划。

完整 15 项矩阵见 [自动测试与验证说明](TESTING.md)。

## 统一质量门禁

日常提交和发布前优先运行：

```powershell
.\tools\run_quality_gates.ps1
```

它顺序执行结构/静态边界校验、Release Host 构建、CTest 和 Master/Slave ARMCC
构建。没有 Keil 的环境可使用 `-SkipFirmware`，但发布验收不能跳过固件构建。

API 文档单独生成：

```powershell
doxygen Doxyfile
```

见 [Doxygen 接口文档](DOXYGEN.md)。

## GitHub 自动检查

GitHub 托管环境执行结构校验和电脑端测试。ARMCC 是商业 Windows 工具链，当前不在公共 runner 上运行；发布前仍必须执行本地双目标 Keil 构建，或后续接入带 Keil 的自托管 Windows runner。

## 烧录约束

主从协议版本必须匹配。协议有破坏性变化时，应先构建两个目标，并在同一次维护操作中烧录 Master 和 Slave。
