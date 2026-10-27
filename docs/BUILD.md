# 构建

## 环境

- 固件：Keil µVision 5、ARMCC 5.06 update 7、SPL，目标 STM32F103C8。
- 主机检查：Python 3.9+、CMake 3.16+ 和 GCC/Clang/MSVC。
- Linux GCC/Clang 运行完整 8 项回归；Windows 原生构建运行 4 项可移植测试。

## 工程检查

```sh
python tools/validate_project.py
```

检查工程文件、源文件/include 路径、平台重复目录、校准 Flash 页和文档链接。

## Linux 主机回归

仓库根目录执行：

```sh
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Windows 可在 WSL 内执行以上命令。寄存器模型使用非 PIE 可执行文件容纳模拟的 32 位 DMA 地址。

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

默认 Keil 路径由脚本参数定义，可通过 `-KeilPath` 覆盖。也可直接打开两个 MCU 目录的 `Project.uvprojx`。
Master 和 Slave 都需达到 0 error / 0 warning；日志保存在各自 `Objects/engineering_build.log`。
STM32F103C8 按 64KB Flash、20KB RAM 预算，从控最后 1KB 留给校准；免费链接器的 32KB 上限需另行检查。
固件链接与资源占用以当前工具链实际输出为准，主机测试不提供固件链接结论。

## 烧录与版本记录

1. 保存当前匹配固件，记录提交、编译器、协议与板卡版本。
2. 拆除桨叶，成对烧录 Slave 和 Master。
3. 检查帧计数、传感器有效性、低油门锁、失联恢复和电机方向。
4. 按[测试与验证](TESTING.md)记录台架结果；回滚也以匹配固件为单位。

协议破坏性变化需增加版本号、更新[协议](PROTOCOL.md)并同时升级两端。

## 常见问题

- Keil 找不到文件：运行结构校验，检查引用路径与已提交文件。
- 主从无数据：检查 Slave PA2 → Master PB11、共地、115200 8N1 和协议版本；观察序号与 CRC 错误。
- CRSF 无有效帧：检查 PA3、420000 baud、DMA 资源和 CRC 计数。
- 电机未放行：按[安全状态](SAFETY.md)检查有效帧、低油门历史和当前状态。

GitHub Actions 对两条长期分支的提交和拉取请求运行工程检查与 Linux 回归；公共 runner 不执行 Keil 链接。
