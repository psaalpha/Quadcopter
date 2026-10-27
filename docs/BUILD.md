# 构建

## 环境

- 固件：Keil µVision 5、ARMCC 5.06 update 7、SPL，目标 STM32F103C8。
- 工程默认使用 C99、`-O2` 和 MicroLIB。
- 主机检查：Python 3.9+、CMake 3.16+ 和 GCC/Clang/MSVC。
- 完整 10 项回归使用 Linux GCC/Clang；Windows 原生构建仅包含可移植的 5 项。

## 工程检查

在仓库根目录执行：

```sh
python tools/validate_project.py
python tools/check_freertos.py
```

前者检查工程文件、源文件/include 路径、重复目录和文档链接；后者检查固定内核、静态分配、中断优先级和异常向量。
存在本机 Keil 链接图和构建日志时，RTOS 检查也核对内存预算；缺少它们时会明确跳过链接与体积结论。

## Linux 主机回归

```sh
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Windows 可在 WSL 内执行以上命令。寄存器模型需要 Linux 非 PIE 可执行文件，以容纳模拟的 32 位 DMA 地址。

## Windows 原生主机测试

MinGW GCC 和 Ninja 已在 PATH 时：

```powershell
cmake -S . -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/windows
ctest --test-dir build/windows --output-on-failure
```

这是可移植测试子集，不能代替 Linux 完整回归。

## 双目标固件

```powershell
.\tools\build_firmware.ps1
```

脚本默认 Keil 路径为 `E:\STM32_Keil_Free\Keil_v5\UV4\UV4.exe`，可覆盖：

```powershell
.\tools\build_firmware.ps1 -KeilPath 'D:\Keil_v5\UV4\UV4.exe'
```

也可直接打开 `Master_MCU/Project.uvprojx` 和 `Slave_MCU/Project.uvprojx`。
两个目标都需达到 0 error / 0 warning；日志写入各自 `Objects/engineering_build.log`。
构建后再次执行 `python tools/check_freertos.py`，核对实际 Flash/RAM 与端口向量。

F103C8 按 64KB Flash、20KB RAM 预算；从控最后 1KB 留给校准参数。
现有免费链接器另有 32KB 上限，从控历史保守余量仅 128B，修改后应重新核对链接图。
MicroLIB 的显示格式化和软浮点路径仍需上板验证。

## 烧录与版本记录

1. 保存当前匹配的主从固件，记录提交、编译器、协议版本和板卡版本。
2. 拆除桨叶，成对烧录 Slave 和 Master。
3. 检查传感器有效性、主从帧计数、低油门锁、失联恢复和电机输出方向。
4. 按[测试与验证](TESTING.md)记录台架结果；回滚也以匹配的两端固件为单位。

协议破坏性变化必须增加版本号、更新[协议文档](PROTOCOL.md)并同时升级两端。

## 常见问题

- Keil 找不到文件：先运行结构校验，确认引用路径和文件已提交。
- 主从无数据：确认 Slave PA2 → Master PB11、共地、115200 8N1 和匹配协议；观察帧数、序号与 CRC 错误。
- CRSF 无有效帧：确认 PA3、420000 baud、DMA 通道和 CRC 计数。
- 电机未放行：检查有效遥控帧、低油门历史和安全状态，按[安全状态](SAFETY.md)定位。

## 自动检查

GitHub Actions 对两条长期分支的提交和拉取请求运行对应的工程检查与 Linux 主机测试；本分支额外检查 RTOS 源码。
公共 runner 不执行 Keil 固件链接。协议、控制和硬件驱动修改仍需双目标构建及相关台架验证。
