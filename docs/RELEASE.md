# 固件发布流程

## 分支约定

- `develop`：日常集成和工程化开发；
- `main`：经过验证的稳定基线；
- 不直接在 `main` 上进行实验性修改。

## 发布候选检查

1. 工作树干净；
2. `develop` 已同步远端；
3. `CHANGELOG.md` 已记录行为和兼容性变化；
4. 协议、引脚、安全和构建文档已更新；
5. 结构校验通过；
6. host tests 全部通过；
7. Master/Slave ARMCC 0 error/0 warning；
8. 无桨测试通过；
9. 明确当前协议版本和匹配固件。

## 构建

```powershell
python tools/validate_project.py
cmake -S . -B build/host -G "MinGW Makefiles"
cmake --build build/host
ctest --test-dir build/host --output-on-failure
.\tools\build_firmware.ps1
```

记录：

- Git commit SHA；
- Master 程序大小；
- Slave 程序大小；
- 编译器版本；
- 协议版本；
- 硬件版本；
- 台架测试结论。

## 主从匹配规则

当前主从协议是显式版本协议，但发布仍采用成对固件：

```text
Release package
├── Master firmware
├── Slave firmware
├── commit SHA
├── protocol version
└── validation record
```

如果协议发生破坏性变化：

- 增加协议版本；
- 更新 `PROTOCOL.md`；
- 增加兼容性测试；
- 在 CHANGELOG 标记必须成对升级；
- 禁止混用旧 Master 与新 Slave。

## 烧录顺序

建议：

1. 拆除桨叶；
2. 保存当前可回滚固件；
3. 烧录 Slave；
4. 烧录 Master；
5. 检查主从帧计数；
6. 检查上电低油门锁；
7. 完成失联和重连测试；
8. 完成传感器和电机方向检查。

## 回滚

回滚必须以 Master/Slave 匹配对为单位。回滚后重新执行：

- 主从通信检查；
- RC failsafe；
- 电机最小输出；
- 传感器有效性；
- 无桨台架测试。

不要只根据“能够启动”判断回滚成功。
