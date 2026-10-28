# 通信与参数

## 主从链路

Slave PA2 → Master PB11，共地，115200 8N1。版本 2 固定 53 字节；主从必须成对升级，版本 1 不兼容。
所有多字节字段为小端序。

| 偏移 | 字节 | 字段 | 说明 |
|---:|---:|---|---|
| 0 | 2 | Magic | A5 5A |
| 2 | 1 | Version | 2 |
| 3 | 1 | Type | 1，传感器 |
| 4 | 1 | Payload length | 44 |
| 5 | 2 | Sequence | uint16，自然回绕 |
| 7 | 2 | Flags | 下表 |
| 9 | 4 | Timestamp | 从控启动毫秒 |
| 13 | 4 | Pressure | int32，Pa |
| 17 | 2 | Temperature | int16，0.01°C |
| 19 | 4 | Baro altitude | int32，相对启动/按键基准，mm |
| 23 | 2 | Heading | uint16，平面磁航向，0.01°；主控使用 XYZ 做倾斜补偿 |
| 25 | 4 | Flow X | int32，累计角增量，1e-4 rad |
| 29 | 4 | Flow Y | 同上 |
| 33 | 2 | Range | uint16，mm |
| 35 | 1 | Quality | 0..100 |
| 36 | 2 | Battery | uint16，mV |
| 38 | 1 | Reserved | 0 |
| 39 | 6 | Magnetic XYZ | 三个 int16，已应用从控偏移和比例校准 |
| 45 | 4 | Flow integration | uint32，累计积分时间，µs，自然回绕 |
| 49 | 2 | Flow epoch | 失效/重启接收后的连续段编号 |
| 51 | 2 | CRC16 | 覆盖 0..50，CCITT-FALSE：1021 / FFFF，不反射、不额外异或 |

| Flags bit | 含义 |
|---:|---|
| 0 | 气压数据有效 |
| 1 | 本帧含新的有效磁样本 |
| 2 | 光流有效 |
| 3 | 从控低电压告警 |
| 4 | 磁力计正在校准 |
| 5 | 电池采样有效 |
| 6 | 磁 XYZ 有效 |
| 7 | 光流已经通过模块帧校验 |

接收端先检查版本、类型、长度和 CRC，再发布数据。控制层拒绝重复序号/时间戳和倒退帧；超时后仅允许低序号、低启动时间的重启流重新建立基准。
CRC 合法的旧帧也不能刷新健康状态。200ms 未收到新数据时退出辅助控制。

## CORVON CV-0850

从控 USART1 PA10 接模块 TX，115200 8N1；模块应设为原生串口协议或 MSP/INAV 输出。
直接使用模块处理后的光流角增量，不进行图像运算。

- 原生帧：19 字节，`DF 15 00 55 seq 0C`；距离位于 6..7，质量 8，X/Y 为 10..13 的 int16，积分时间 14..15 为 µs，有效标志 16 必须为 245，版本 17，18 为前 18 字节累加和。
- MSPv2：`$X<`，flags=0，功能 `1F01` 的 payload 为质量+int32 距离，`1F02` 为有效标志+int32 X/Y；CRC8 DVB-S2 多项式 D5，覆盖 flags 到 payload 末尾。
- 原生光流单位为 1e-4 rad；位移 mm = 角增量 × 0.0001 × 高度 mm。导航还处理安装偏转角、轴尺度、机体旋转和航向坐标转换。
- MSP 兼容配置沿用 CV-0850 的该角增量尺度，并按模块 50Hz 使用 20000µs；该配置仍需通过实机平移核对尺度和正负号。若模块输出语义不同，先更正解析配置，不能直接启用导航。
- 每个有效模块帧都累加，20Hz 主从发布不会只保留最后一帧。校验错误、接收溢出、校准暂停或 200ms 超时会使连续段失效；恢复时重新建立位置基准。

依据：[CORVON CV-0850 官方规格](https://www.corvon.tech/spec-sheets/en/corvon-cv0850-spec-sheet-en.html)。当前未实现 MAVLink 解析。

## 蓝牙命令

Master USART1，38400 8N1，ASCII 方括号包；一帧最多 99 个正文字符。命令与遥测均以 `]` 结束。

```text
[slider,PKp,0.179]
[set,PAKp,6.03]
[get,PAKp]
[save]
[load]
[defaults]
[calibrate,gyro]
[zero,yaw]
[status]
[bench,1]
[set,Contrl_Speed,10]
[bench,0]
```

`slider` 与 `set` 同义。只接受完整有限十进制数，可用 e±0..6。成功调参/读取返回 `[value,名称,六位小数]`；动作返回 `[ok,操作号]`；拒绝返回 `[error,...]`。
`save/load/defaults/gyro/zero/bench` 操作号依次为 2..7，`status` 为 8。准确编号以 `flight_parameters.h` 为准。
保存、加载、恢复默认、校准和归零要求 DISARM 且油门 ≤5%；安装、比例、悬停、电压、导航使能和零偏参数仅允许停机修改。

| 参数 | 作用 | 持久化 |
|---|---|---|
| PKp/PKi/PKd，RKp/RKi/RKd，YKp/YKi/YKd | 三轴角速度 PID；旧协议 P/R 的 Kd 在控制器中乘 0.01 | 是 |
| PAKp/RAKp/YAKp | 角度外环增益 | 是 |
| Mid | 横滚/俯仰遥控中心修正，-10..10 通道单位 | 是 |
| PAIM/RAIM | 叠加的俯仰/横滚角目标，±30° | 否 |
| AKp/AKi/AKd | 高度 PID，误差 cm，输出油门百分比 | 是 |
| XKp/XKi/XKd，YKpPos/YKiPos/YKdPos | 水平位置 PID，误差 cm，输出角度 | 是 |
| FlowXScale/FlowYScale | 光流有符号尺度，-5..5，绝对值至少 0.01 | 是 |
| FlowYaw | 模块相对机体的安装偏角，±180° | 是 |
| Hover | 辅助模式悬停基准油门，5..80% | 是 |
| BatWarn/BatCrit | 告警/禁止解锁阈值，必须 Warn > Crit | 是 |
| RollOffset/PitchOffset/Declination | 安装角和磁偏角 | 是 |
| NavEnable | 0/1；启用前 AKp、XKp、YKpPos 必须均 >0 | 是 |
| GyroX/GyroY/GyroZ | 原始陀螺仪零偏；校准完成会更新，需 save 才永久保存 | 是 |
| Contrl_Speed | 无桨试转油门，0..20%；只在 bench 模式接受 | 否 |

参数范围由唯一的 `flight_parameters.c` 表定义。导航默认关闭，辅助 PID 默认零增益。Flash 使用两页交替、版本、CRC 和最后提交标记；写入中断时保留上一条完整记录。
`load` 恢复最后保存值，`defaults` 只恢复 RAM，需要再 `save` 才覆盖持久配置。恢复参数不会恢复试转油门或临时姿态目标。

## 遥测

每 100ms 轮换一页：

- `[att,r,p,y,gx,gy,gz]`：角度和角速度 ×100。
- `[state,s,b,v,h,d,c,m,q,r]`：安全状态、电池 mV、电池级别、健康位、未保存标志、陀螺校准状态、磁拒绝数、旧帧拒绝数、启动 RCC 复位标志。
- `[motor,FL,FR,BL,BR]`：实际四路 CCR，500..1000。
- `[nav,x,y,h,vx,vy]`：位置 cm、高度 cm、速度 cm/s。
- `[plot,p]`：俯仰角 ×10，保留原绘图用途。

电池级别 0=正常、1=告警、2=临界/无有效数据；健康位 0=IMU 连续有效、1=从控新鲜、2=辅助模式运行、3=bench。
陀螺校准 0=未开始、1=采集中、2=成功、3=失败；不运动采集 256 点，运动或超时拒绝。
