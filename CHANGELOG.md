# Changelog

## 2026-10-27 - Repository cleanup

- 裸机版本持续维护于 baremetal，FreeRTOS 版本维护于 freertos。
- 工程文档收拢为架构、构建、测试、引脚、协议和安全六份，统一当前版本说明。
- 自动检查使用当前文档路径，CI 覆盖两条长期分支。
- Linux Release 构建与 8 项主机回归通过；固件、协议、控制参数和第三方依赖未修改。

## 2026-10-08 - Source layout

- 整理 Core、App、BSP、Control 和 Drivers 目录，统一项目自有命名与 Keil 工程引用。
- 保留公共平台与 Bosch 源码内容。

## 2026-10-04 - Scheduling and I/O

- 主从统一毫秒时基与周期发布，串口解析移出中断。
- 完善 DMA 缓冲所有权、从控校准和分段显示。

## 2026-07-26 - Host tests

- Release 构建保持断言启用，补齐工程、协议、构建与验证文档。

## 2026-07-25 - Firmware structure and safety

- 共享 STM32F1 平台代码，主从协议增加版本、定宽字段、序号与 CRC16。
- 主控引入周期调度、安全状态机、板级资源配置和主机测试。
- CRSF 增加 CRC8、300ms 超时、低油门启动与恢复锁，修复蓝牙 PID 参数更新。

## 2026-06-28 - PID

- 偏航混控接入四路输出，增加高度和光流 PID 计算接口。
- 辅助 PID 默认增益保持为零。
