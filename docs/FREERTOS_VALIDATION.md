# FreeRTOS 迁移验证记录 2026年10月16日

本次从远端 `codex/resume-study-spl` 最新提交
`3d4a73d9be58bc02c4fcd5acf1b60bccdce97213` 建立
`feature/freertos-study-migration`。原分支与main未修改，未合并、未强推。
简历单独保存在本地输出目录，个人PDF未放入仓库。

## 已实际完成

- Keil MDK、ARM Compiler 5.06 update 7 build 960，两目标编译及链接均0 error / 0 warning。
- 默认工程使用ARMCC `-O2`、MicroLIB、C99和SPL，无内核运行期动态分配。
- Linux WSL GCC 13.3.0、CMake/CTest Release：10项回归全部通过，编译警告视为错误。
- `validate_project.py`：双Keil源文件/选项列表、include、共享协议、校准页边界、文档链接检查通过。
- `check_freertos.py`：固定内核文件校验、IRQ数值、异常向量归属、无heap_x.c、延时不占用SysTick，
  以及本地Keil map的Flash/RAM预算检查通过。
- `git diff --check`无空白错误；Git在Windows提示的LF/CRLF转换不代表编译失败。
- 原协议实现、PID/姿态计算实现和纯安全状态机文件没有改动。

10项分别是task_health、crsf_io、inter_mcu_protocol、flight_safety、pid_control、yaw_estimator、
master_io、scheduler、slave_io、slave_app。Windows原生MinGW只运行其中可移植测试，
需要32位DMA地址的寄存器模型测试在WSL中实际运行；不能把仅Windows的子集冒充全部回归。

新增CRSF回归覆盖分段帧、CRC错误、无IDLE连续流、跨环形边界、整圈覆盖丢弃、DMA TE后的重同步。
新增看门狗回归覆盖后台饿死、服务任务停滞、门限边界及uint32时基回绕。
原回归继续覆盖41字节协议、低油门/300ms失联恢复、PID重置、DMA busy所有权、校准和逐字符显示。
这些回归不运行真实Cortex-M调度器，也不量测硬件中断延迟。

## Flash RAM 与32KB链接器

按ST数据手册，F103C8为64KB Flash、20KB SRAM，不能按部分板卡可能存在的额外Flash扩容。
从控继续预留最后1KB `0x0800FC00..0x0800FFFF`，应用IROM上限为63KB。

| 本次最终链接图指标 字节 | Master | Slave |
|---|---:|---:|
| Code | 24,172 | 30,280 |
| RO data | 508 | 2,016 |
| RW data 未压缩 | 684 | 344 |
| ZI data | 9,668 | 10,696 |
| RO合计 | 24,680 | 32,296 |
| ROM合计 含压缩初值 | 24,784 | 32,340 |
| Load Region未压缩尺寸 保守预算 | 25,364 | 32,640 |
| RAM RW加ZI | 10,352 | 11,040 |
| SRAM余量 相对20,480 | 10,128 | 9,440 |
| 相对32,768的保守余量 | 7,404 | 128 |

RAM包含静态任务栈、TCB、队列、DMA buffer、1024B启动/异常MSP和原512B C库heap保留区。
静态内存总量合格不等于每个任务的栈深足够；PSP高水位和MSP嵌套必须上板量测。
从控当前虽已通过免费版链接，但保守余量仅128B。换编译器、改变优化或增加代码后
可能再次超限；不要把64KB芯片容量与工具授权的32KB限制混为一谈。

本机首次核验原裸机基线时，Master报告33,004B、Slave报告34,930B，已因免费版32KB限制失败。
迁移中保留完整C库、启用-O2的配置仍失败：Master 33,908B、Slave 36,362B。
最终启用MicroLIB后，两目标才获得真实成功的链接结果。没有绕过授权或伪造成功。
MicroLIB改变C库选择，需复核软浮点、显示格式化及所有使用C库的硬件路径。

原始文本证据在 [validation目录](validation)。链接图摘要包含ROM/RAM及SVC、PendSV、SysTick
来自官方port.o的符号信息。临时试编失败日志只说明当时配置的限制，不是最终源码可用性的证据。

## 可复现步骤

```powershell
python tools/validate_project.py
cmake -S . -B build/host -G Ninja
cmake --build build/host
ctest --test-dir build/host --output-on-failure
.\tools\build_firmware.ps1
python tools/check_freertos.py
```

Keil默认路径为 `E:\STM32_Keil_Free\Keil_v5\UV4\UV4.exe`，其他机器使用 `-KeilPath` 参数。
Linux完整回归使用 `cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release`，然后build与ctest。
CI只提供源码/主机门禁，不把没有Keil的GitHub Actions说成固件链接验证。

## 尚未完成

- 未烧录两块实际F103，未测2ms控制周期、最坏执行时间、ISR到任务延迟或PWM电气行为。
- 未测任务PSP/MSP余量、饥饿后的真实IWDG复位、栈hook/assert/CPU异常最小输出。
- 未验证MicroLIB下的硬件显示与软浮点路径，未做DMA高负载、Flash校准和输入边沿台架测试。
- 未确认光流模块校验规则及物理量尺度，未改变导航零增益默认值或增加正式ARM开关。
- 无桨台架、受控飞行和调参均未执行；本分支不能标记为可安全实飞。

具体测试次序见 [教学文档末节](FREERTOS_MIGRATION.md#8-尚需完成的无桨验证)。

## 简历更新的事实边界

已实际读取用户提供的本地PDF并渲染其一页原版，保留姓名、照片、联系方式、教育、
三段实习、另一个项目、奖项和其余原文。只替换飞控项目的裸机迁移规划及技能补充，
写入“学习迁移、Keil零警告构建、10项主机回归通过、尚未上板验证”。
原简历中的示波器/逻辑分析仪经历为用户原文保留，不代表本次RTOS迁移也已做过这些实测。
原PDF没有覆盖；新版仍一页，另提供可直接打开编辑文字的HTML版本。
