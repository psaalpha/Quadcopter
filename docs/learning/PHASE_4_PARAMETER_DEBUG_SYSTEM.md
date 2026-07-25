# Phase 4 学习指南：参数和调试系统

## A. 这个模块解决什么工程问题

只有“ID + 值”的参数无法支持长期维护和地面站。工程还需要分类、名称、单位、
版本可用性、默认值/范围一致性，以及掉电保存时的并发保护。

## B. 设计思想

- 不改变稳定 `ParameterStore_*` 接口；
- 描述值规则的 Descriptor 与描述工具语义的 Catalog 分开；
- Catalog 和 Store schema 必须一致；
- 保存使用 revision 快照，防止旧镜像把新修改标记为已保存；
- Flash 行为通过 `read/write_atomic` 后端注入；
- 原子写、双槽和断电恢复是后端责任；
- 控制参数只在明确提交点生效。

## C. 涉及嵌入式知识

- 参数元数据和稳定 ID；
- schema evolution；
- CRC、默认值和范围校验；
- Flash erase/program 限制；
- wear leveling 与双槽提交；
- 异步保存和 stale revision；
- 串口调参的数据模型；
- 配置和运行状态分离。

## D. 面试如何描述

> 我保持原 ParameterStore API 不变，在外侧增加 ParameterCatalog，给参数提供
> category、稳定 name、unit 和 introduced/deprecated schema。持久化由
> ParameterPersistence 协调，先记录 revision、编码带 CRC 的镜像，再调用
> write_atomic 后端，最后只在 revision 未变化时清除 dirty，从而避免异步保存
> 覆盖并发调参。

## E. 后续我需要自己学习什么

1. STM32F1 Flash page 擦写和执行停顿；
2. 双槽 generation/commit marker；
3. 掉电注入测试；
4. 参数 schema migration；
5. 参数权限、只读和 reboot-required；
6. 地面站参数树与缓存；
7. PID 参数原子快照和 bumpless transfer。
