# 构建

## 完整固件

推荐 Arm GNU Toolchain（本地验证版本 15.2.Rel1）。Python 3.9+ 从 Keil 工程读取同一份源文件、宏和 include 配置：

```sh
python tools/build_gcc.py
```

编译器不在 PATH 时：

```powershell
python tools/build_gcc.py --compiler 'D:\STM32\ARM\bin\arm-none-eabi-gcc.exe'
```

构建 Master、Slave，警告视为错误；输出在 `build/firmware-gcc/{Master_MCU,Slave_MCU}/`，含 ELF、HEX、BIN、map。
`summary.json` 记录编译器、映像长度、RAM 和向量检查结果。脚本检查复位向量、MSP 和异常向量；链接器限制 62KB 应用 Flash / 20KB RAM。
RAM 包含静态数据、任务栈（如有）、512B 预留 heap 和 1024B MSP；固件禁止动态 heap 分配，显示与参数解析均不依赖浮点 printf/strtod。

两块板均保留 `0x0800F800`、`0x0800FC00` 两个 1KB 参数页。不能用其他链接脚本把程序放入这些页。
兼容旧 CMSIS 的三条 STREX 约束只在 GCC 构建副本修正；供应商源码保留原样。
FreeRTOS 分支自动选择 GCC ARM_CM3 端口；Keil 工程使用固定版本的 RVDS 端口。

## Keil

保留 µVision5、ARMCC5、SPL、C99、MicroLIB 工程：

```powershell
.\tools\build_firmware.ps1
```

可用 `-KeilPath` 覆盖路径，也可直接打开两份 `Project.uvprojx`。脚本要求两块板均 0 error / 0 warning。
现有免费 ARMCC 链接器有 32KB 限制，完整功能的部分目标会超过该限制；使用 GNU 工具链或具有相应容量许可的 Keil。免费版链接失败不构成固件已构建的证据。
本次完整固件验证使用 GNU，具体资源见[测试记录](TESTING.md)。

## 工程与主机测试

```sh
python tools/validate_project.py
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

FreeRTOS 分支另执行 `python tools/check_freertos.py`，检查固定内核哈希、静态分配、中断优先级和端口向量归属。
Linux GCC/Clang 完整 15 项；Windows 可用 WSL 运行同样检查。Windows 原生仅编译 10 项可移植测试，寄存器模型使用 Linux 非 PIE 地址布局。
主机测试没有仿真实际 ARM 指令时序、传感器电气行为、内核切换或飞行动力学。

GitHub Actions 对两条分支执行工程检查、15 项 Linux 测试和双目标 GNU 固件构建，并保存固件构建产物。

## 烧录

记录分支、提交、编译器与板卡版本；拆桨后成对烧录对应分支的 Master/Slave。
不要混用 v1/v2 或两次不匹配构建的主从固件。先核对 CH8 解锁、传感器状态、电压、轴向和输出方向，再按[安全检查](SAFETY.md)进行台架验证。
参数页会在整片擦除时丢失；普通程序下载是否保留参数取决于烧录器擦除设置。首次启动使用默认参数，导航关闭。
