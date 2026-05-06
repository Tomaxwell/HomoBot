---
name: design-review
description: 对指定设计文档进行架构 + 安全双重审查
argument-hint: <设计文档路径或模块缩写>
---

# 设计审查流程

**审查目标**：$ARGUMENTS

## 执行步骤

### Step 1：加载文档

读取目标设计文档和系统架构上下文（`CLAUDE.md`、`AGENTS.md`）。目标可为 `codex_*_design.md` 或 `claude_*_design*.md`；审查 Claude Code 文档时只输出审查意见，不直接覆盖原文。

### Step 2：并行审查

**同时**使用 Agent 工具启动两个审查：

**审查 A — 安全审查**（调用 `safety-validator`）：
```
请对以下设计文档进行安全审查，重点检查：
E-Stop 路径、状态机故障转换、模块权限边界、接口安全性。

{设计文档内容}
```

**审查 B — 架构审查**（调用 `architecture-advisor`）：
```
请对以下设计文档进行架构审查，重点检查：
是否符合系统架构原则（单一状态源、进程治理中枢、云端入口唯一等）、
通信协议选择是否合理、接口设计是否与其他模块一致。

{设计文档内容}
```

### Step 3：综合报告

将两份审查结果汇总，输出：

```
## 设计审查综合报告

**模块**：{模块名}
**总体判决**：APPROVED | APPROVED_WITH_CONDITIONS | NEEDS_REVISION

### 安全审查结果
{safety-validator 输出}

### 架构审查结果
{architecture-advisor 输出}

### 综合建议
{优先级排序的修改建议列表}
```

将报告写入单独文档，命名为 `codex_{target}_design_review.md`；不要直接追加或修改 `claude_` 设计文档。
