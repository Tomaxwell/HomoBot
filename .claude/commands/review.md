---
name: review
description: 对当前目录下修改的 ROS2 C++ 代码进行代码审查
---

# 代码审查流程

## 执行步骤

### Step 1：确定审查范围

运行 `git diff --name-only` 获取修改的文件列表。
如果不在 git 仓库中，询问用户指定要审查的文件。

### Step 2：读取代码

读取所有修改的 `.cpp`、`.hpp` 文件内容。

### Step 3：委托 code-reviewer

使用 Agent 工具调用 `code-reviewer`，传入：
- 文件内容
- 所属模块（从文件路径推断）
- 系统架构上下文（CLAUDE.md 中的相关约束）

### Step 4：输出审查报告

输出审查报告，格式：
```
## 代码审查报告

**文件**：{文件列表}
**评级**：APPROVED / APPROVED WITH SUGGESTIONS / NEEDS REVISION

### 阻塞问题
（如无则省略）

### 建议项
（如无则省略）

### 亮点
```
