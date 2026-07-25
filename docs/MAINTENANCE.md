# 运行与维护手册

## 日常健康检查

每次开始开发前：

```powershell
git status --short --branch
python tools/validate_project.py
ctest --test-dir build/host --output-on-failure
```

发布或烧录前再执行：

```powershell
.\tools\build_firmware.ps1
```

## 常见问题定位顺序

### Keil 找不到文件

1. 运行 `python tools/validate_project.py`；
2. 检查 `.uvprojx` 中路径是否相对工程目录；
3. 检查文件是否只在本地存在但未提交；
4. 检查是否重新创建了 `Master_MCU/Library` 等旧目录。

### Master 收不到 Slave

1. 确认 PA2（Slave TX）连接 PB11（Master RX）并共地；
2. 确认两端 115200 8N1；
3. 确认 Master/Slave 固件来自相同协议版本；
4. 观察 `frames_received`、`crc_errors`、`format_errors`；
5. 检查 Slave 发送序号和时间戳是否前进；
6. 不要先修改 PID 或传感器算法。

### CRSF 失联

1. 检查 PA3 RX 和 420000 baud；
2. 查看有效帧计数与 CRC 错误计数；
3. 确认 RC DMA 通道没有被其他模块占用；
4. 验证 `system_tick_5ms` 正常前进；
5. 无桨检查 LINK_LOSS 和 RECOVERY_LOCK 行为。

### 电机输出不放行

依次确认：

1. 是否收到通过 CRC 的 RC 帧；
2. 油门是否曾经降至 5% 或以下；
3. 当前安全状态是否为 ACTIVE；
4. 是否刚发生过失联；
5. TIM4 四通道是否保持在安全最小值；
6. 不要通过注释掉安全逻辑解决问题。

### 传感器数值异常

1. 确认数据有效性 flag；
2. 确认物理单位；
3. 确认主从协议版本；
4. 比较原始值与转换值；
5. 检查总线超时、供电和接地；
6. 最后才检查滤波和控制参数。

## 诊断数据

当前可利用：

- CRSF 有效帧数和 CRC 错误数；
- 主从接收帧数、CRC 错误、格式错误和序号间断；
- scheduler task overrun；
- safety failsafe count；
- Slave 协议时间戳和状态 flags。

新增故障处理时，应优先增加计数或状态，而不是只增加 LED 闪烁。

## 硬件修改维护

任何引脚变更必须同步检查：

- GPIO 模式和复用；
- APB/AHB 时钟；
- DMA 通道；
- NVIC 优先级；
- 调试接口占用；
- `PINOUT.md`；
- Master/Slave 接线关系。

## 安全提醒

- 电机相关调试默认拆除桨叶；
- 不在飞行中验证新的 failsafe；
- 不使用“暂时关闭看门狗/CRC/低油门锁”作为长期修复；
- 保留最后一个经过验证的 Master/Slave 匹配固件；
- 协议不兼容时禁止只烧录其中一颗 MCU。
