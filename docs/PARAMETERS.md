# 参数管理系统

## 1. 当前范围

`Shared/Services/parameter_store.*` 提供与硬件无关的参数核心：

- 参数描述表；
- 类型和范围校验；
- 默认值恢复；
- dirty 与 revision 管理；
- 带 schema version 和 CRC32 的持久化镜像；
- 事务式解码；
- 电脑端单元测试。

本阶段没有：

- 接管现有 PID 调参入口；
- 写入 STM32 内部 Flash；
- 在飞行中擦除 Flash；
- 定义最终产品参数 ID；
- 改变任何控制默认值。

这样可以先验证数据模型和损坏处理，再单独评审飞控参数迁移。

## 1.1 扩展模块

现有 `ParameterStore_*` 函数签名保持不变。扩展能力位于外侧：

| 模块 | 职责 |
|---|---|
| `parameter_catalog.*` | 分类、稳定名称、单位、引入和废弃 schema |
| `parameter_persistence.*` | 调用编码/解码并协调原子存储后端 |

分类包括 System、Sensor、Control、Communication、Safety 和 Debug。名称建议
使用稳定点分格式，例如 `control.roll_kp`、`communication.timeout_ms`。

Catalog 必须与 Store 使用相同 schema version，且每个 Descriptor 都必须有
且只有一个可用 metadata。Catalog 不保存当前值。

## 2. 参数描述表

每个参数必须定义：

```c
typedef struct
{
    uint16_t id;
    ParameterType type;
    ParameterValue default_value;
    ParameterValue minimum;
    ParameterValue maximum;
    uint16_t flags;
} ParameterDescriptor;
```

规则：

- ID 在同一 schema 中唯一，发布后不能改变含义；
- 类型支持 `float32`、`int32` 和 `uint32`；
- 默认值必须位于上下界内；
- 运行时修改需要 `PARAMETER_FLAG_RUNTIME_WRITABLE`；
- 需要保存的参数标记 `PARAMETER_FLAG_PERSISTENT`；
- 不能立即生效的参数标记 `PARAMETER_FLAG_REBOOT_REQUIRED`；
- 描述表和当前值均由调用方静态分配。

## 3. 生命周期

```text
ParameterStore_Init
    |
    +-- 使用默认值，dirty = true，revision = 1
    |
    +-- Decode(valid image)
    |      -> 应用持久化值，dirty = false
    |
    +-- Decode(invalid image)
           -> 保留默认值或当前值，由上层记录故障
```

运行期：

```text
Set
  -> 查找 ID
  -> 检查类型
  -> 检查 writable
  -> 检查范围
  -> 值变化时 revision++ / dirty=true
```

相同值重复写入不会增加 revision。

## 4. revision 与异步保存

保存流程必须记录导出镜像对应的 revision：

1. 读取 `ParameterStore_GetRevision()`；
2. 调用 `ParameterStore_Encode()`；
3. 后端完成原子保存；
4. 调用 `ParameterStore_MarkPersisted(saved_revision)`。

如果保存期间参数再次变化，当前 revision 已不同，
`ParameterStore_MarkPersisted()` 返回 `PARAMETER_STATUS_STALE_REVISION`，
dirty 不会被错误清除。

## 5. 持久化镜像格式

所有多字节整数使用 little-endian。

### 5.1 Header（16 bytes）

| Offset | Size | 字段 |
|---:|---:|---|
| 0 | 4 | Magic `QPAR` |
| 4 | 2 | Schema version |
| 6 | 2 | Entry count |
| 8 | 2 | Payload length |
| 10 | 2 | Reserved，当前为 0 |
| 12 | 4 | Payload CRC32 |

### 5.2 Entry（8 bytes）

| Offset | Size | 字段 |
|---:|---:|---|
| 0 | 2 | Parameter ID |
| 2 | 1 | Parameter type |
| 3 | 1 | Reserved，当前为 0 |
| 4 | 4 | IEEE-754 float32 或 32-bit integer 原始值 |

CRC 使用标准 reflected CRC32：

- 初值 `0xFFFFFFFF`；
- 多项式 `0xEDB88320`；
- 结果最终异或 `0xFFFFFFFF`；
- 只覆盖全部 entry payload。

## 6. 事务式加载

`ParameterStore_Decode()` 在改变当前值前验证完整镜像：

1. Magic、schema、长度和 count；
2. CRC32；
3. 每个描述符恰好出现一次；
4. ID 和类型匹配；
5. 所有值都在当前描述表范围内。

只有全部通过后才统一应用。即使镜像前几个参数有效、后一个越界，当前参数也
不会发生部分更新。

## 7. NVM 后端设计要求

后续 Flash HAL 不能直接把单份镜像原地覆盖。推荐双槽：

```text
Slot A: generation + image + commit marker
Slot B: generation + image + commit marker
```

保存步骤：

1. 选择非当前槽；
2. 擦除；
3. 写入 generation 和参数镜像；
4. 回读并校验；
5. 最后写 commit marker；
6. 下次启动选择 generation 更新且完整的槽。

断电发生在任一步骤时，至少保留上一份有效槽。

约束：

- 仅在 DISARM 状态允许擦写内部 Flash；
- 控制任务不得等待擦除或写入；
- 擦写错误必须进入日志和维护诊断；
- Flash endurance 需要节流和变更合并；
- schema 升级必须提供迁移函数或明确恢复默认值。

`ParameterPersistenceBackend` 已预留：

- `read(context, buffer, capacity, read_size)`；
- `write_atomic(context, buffer, size)`。

`ParameterPersistence_Save()` 会：

1. 记录当前 revision；
2. 编码到调用方 scratch；
3. 调用 `write_atomic`；
4. 仅在 revision 仍一致时清除 dirty。

如果保存过程中又发生参数修改，返回 `STALE_REVISION` 并保持 dirty。
STM32 Flash adapter 尚未实现，避免在未完成双槽和断电测试前误用单页原地写。

## 8. PID 参数迁移步骤

PID 迁移必须独立提交：

1. 固定参数 ID、单位、默认值和范围；
2. 读取现有实际默认值并建立回归测试；
3. 通信解析只写 ParameterStore，不直接写 PID；
4. 参数服务在安全边界生成完整快照；
5. 控制算法只在批准时点一次性应用快照；
6. 无桨台架对比迁移前后的输出；
7. 最后才启用 NVM 加载。

在上述步骤完成前，现有 PID 数据路径保持不变。
