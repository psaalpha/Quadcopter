# Doxygen 接口文档

## 目标

Doxygen 用于生成公共接口、类型、调用边界和 Markdown 架构文档的可浏览版本。
它不是设计文档的替代品，也不为未验证的遗留模块自动补充事实。

## 生成

安装 Doxygen 后，在仓库根目录执行：

```powershell
doxygen Doxyfile
```

入口输出：

```text
build/doxygen/html/index.html
```

告警记录：

```text
build/doxygen-warnings.log
```

输出位于 `build/`，不提交 Git。配置将 Doxygen 文档错误视为失败，但暂不要求
所有遗留 symbol 都有注释；新 public API 必须按 [C 编码规范](CODING_STANDARD.md)
提供 `@file`、`@brief`、单位、上下文和错误语义。

## 输入边界

- `Shared`：协议、HAL、驱动、安全和服务；
- `Master_MCU/App`：应用策略与任务计划；
- `Master_MCU/BSP`：板级契约；
- `docs` 和根 README：架构与维护文档；
- `CONTRIBUTING.md` 与 `tests/host/README.md`：协作和测试扩展规则；
- 不扫描 CMSIS、SPL、Objects、Listings 和 build 输出。

## 审查清单

- 模块职责是否与架构文档一致；
- API 是否暴露 STM32 私有类型；
- 参数单位和范围是否清楚；
- callback/context 的 lifetime 是否清楚；
- 阻塞、ISR-safe 和线程安全属性是否清楚；
- 错误返回和输出有效性是否清楚；
- 新页面是否出现断链或 Doxygen warning。

## CI 策略

GitHub 质量工作流安装固定发行渠道提供的 Doxygen 并执行 `doxygen Doxyfile`。
如果未来工具升级产生新告警，应先审查配置或文档问题，再升级固定基线。
