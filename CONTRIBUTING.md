# 协作规范

## 分支

- `main`：经过验证的稳定版本。
- `develop`：工程化集成分支。
- 功能或修复分支从 `develop` 创建，验证后再合入。

## 提交要求

每个提交只表达一个可验证的工程意图，并使用动词开头的英文提交标题。任何行为、接口、协议、构建或安全策略变化都必须同步更新 `CHANGELOG.md`。

提交前至少执行：

```powershell
python tools/validate_project.py
cmake -S . -B build/host -G "MinGW Makefiles"
cmake --build build/host
ctest --test-dir build/host --output-on-failure
.\tools\build_firmware.ps1
```

## 代码边界

- `Platform` 不得依赖 Master 或 Slave。
- `Shared` 不得包含 STM32 寄存器或具体板级依赖。
- `App` 负责策略，不直接硬编码引脚。
- `BSP` 负责时钟、定时器和板级资源映射。
- `Hardware` 负责设备驱动和控制算法，不承担系统状态切换。
- 中断只完成采样搬运、时间记账、任务通知和必要的硬件应答。

## 安全相关修改

涉及电机输出、遥控、解锁、失联保护、看门狗或 PID 的修改，必须：

1. 描述故障模式和安全默认值。
2. 增加或更新电脑端测试。
3. 完成 Master/Slave ARMCC 构建。
4. 先无桨测试，再进行受控台架测试。
