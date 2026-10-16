# FreeRTOS 固定版本依赖

本目录收录 FreeRTOS-Kernel V11.1.0 的最小构建子集，采用 MIT 许可证。

- 上游仓库 https://github.com/FreeRTOS/FreeRTOS-Kernel
- 标签 `V11.1.0`
- 对应提交 `dbf70559b27d39c1fdb68dfb9a32140b6a6777a0`
- 收录 `tasks.c`、`queue.c`、`list.c`、完整 `include/`、`portable/RVDS/ARM_CM3/`
- 上游文件保持原样；许可证见 `Kernel/LICENSE.md`
- 不依赖 submodule 或运行时下载，两个 Keil 工程直接编译这些文件
- 不加入任何 heap_x.c；静态任务、队列和 idle 内存在链接时确定

`Config/` 是本项目配置和桥接，不是上游源码。教学入口见
[FreeRTOS 迁移说明](../../docs/FREERTOS_MIGRATION.md)。
