---
name: module-design
description: 为指定模块生成完整详细设计文档
argument-hint: <模块缩写> [--safety-review]
---

# 模块详细设计流程

**目标模块**：$ARGUMENTS

## 执行步骤

### Step 1：加载上下文
读取以下文件建立背景知识：
- `CLAUDE.md`（系统架构、18个模块清单）
- `AGENTS.md`（Codex/Claude Code 并行协作规则）
- 已有设计文档（`claude_*_design*.md`、`codex_*_design*.md`）了解已定义的接口模式

### Step 2：委托 module-designer 生成设计

使用 Agent 工具调用 `module-designer`，传入以下 prompt：

```
请为 {模块缩写} 模块（{模块全称}：{模块职责}）生成完整的详细设计文档。

参考已有设计文档的接口风格（如 claude_em_design_v2.md、claude_sm_design.md）保持一致性。
这是 Codex 新设计模块，所有新产物必须使用 codex_ 前缀：
- 设计文档：codex_{module_lower}_design.md
- 接口包：codex_{module_lower}_msgs
- 实现包：codex_{module_lower}
- 自定义 Topic/Service/Action：/codex_{module_lower}/...
不要重命名、覆盖或删除 claude_ 开头的 Claude Code 设计产物。

要求：
1. 完整的状态机设计（如适用）
2. 完整的 ROS2 接口定义（msg/srv/action 文件内容 + 汇总表）
3. 内部节点结构
4. 与其他模块的交互矩阵
5. 关键参数 YAML
6. 错误码定义
7. 包结构

输出完整的 Markdown 文档。
```

### Step 3：保存设计文档

将生成的设计文档保存为 `codex_{module_lower}_design.md`。

### Step 4：安全审查（如包含 --safety-review 或模块涉及运动控制）

使用 Agent 工具调用 `safety-validator`，对生成的设计文档进行安全审查。
将审查结果追加到设计文档末尾的 `## 安全审查` 章节。

### Step 5：汇报

告知用户：
- 设计文档路径
- 设计涉及的 ROS2 接口数量（Topic/Service/Action）
- 安全审查结果（如执行了审查）
- 建议下一步（如：可用 `/ros2-interface {模块缩写}` 生成实际接口文件）
