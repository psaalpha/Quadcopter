# Phase 7 学习指南：工程质量

## A. 这个模块解决什么工程问题

嵌入式项目不仅要“能编译”，还要让接口、约束、验证证据和变更历史可重复。
本模块把文档、可移植边界、警告、测试和双固件构建组织成统一质量门禁。

## B. 设计思想

- Doxygen 管 API，Markdown 管设计、决策和操作流程；
- 公共模块禁止平台头、动态内存和格式化 stdio；
- Host 编译把 warning 当 error，ARMCC 发布基线要求 0/0；
- 自动检查只承诺它实际覆盖的内容；
- 第三方平台代码与项目自有代码分开治理；
- 遗留债务用 baseline 隔离，新代码不新增告警；
- 每个 Phase 独立 commit，并记录目的、知识和价值；
- 文档、测试和代码作为同一个变更单元审查。

## C. 涉及嵌入式知识

- Doxygen 和 API contract；
- GCC/Clang/ARMCC warning 差异；
- Cppcheck、Clang analyzer 和商业静态分析；
- MISRA C deviation；
- CI、可复现构建和 release evidence；
- traceability、change control 和 code review；
- Host test、HIL、soak test 的分层；
- Flash/RAM map 与零告警策略。

## D. 面试如何描述

> 我建立了从仓库结构校验、Host warnings-as-errors、15 项单元测试到主从 ARMCC
> 0 error/0 warning 的统一门禁。Shared 层自动禁止 STM32 平台依赖、动态内存和
> 格式化 stdio；公共接口用 Doxygen，架构和操作流程用 Markdown。我也明确静态检查
> 不覆盖实时性和硬件行为，后者由 WCET、HIL、故障注入和无桨耐久验证补足。

## E. 后续我需要自己学习什么

1. CMake compile database 与 clang-tidy；
2. Cppcheck addon 和 suppressions 管理；
3. MISRA C:2012/2023 与 deviation record；
4. linker map、stack usage 和 binary reproducibility；
5. unit/component/HIL/soak/release evidence 分层；
6. code review checklist 和 defect taxonomy；
7. SBOM、第三方版本固定和供应链安全；
8. 固件签名、bootloader 回滚和现场诊断。
