---
name: ros2-interface
description: 根据模块设计文档生成实际的 ROS2 接口文件（.msg/.srv/.action）和 msgs 包构建文件
argument-hint: <模块缩写>
---

# ROS2 接口文件生成流程

**目标模块**：$ARGUMENTS

## 执行步骤

### Step 1：读取设计文档

读取 `codex_{module_lower}_design.md`，提取所有接口定义章节。不要从 `claude_*.md` 生成 Codex 接口文件，除非用户明确要求迁移。

### Step 2：委托 ros2-interface-definer 生成文件

使用 Agent 工具调用 `ros2-interface-definer`，传入设计文档中的接口定义，要求：

1. 生成所有 `.msg` 文件（含字段注释）
2. 生成所有 `.srv` 文件
3. 生成所有 `.action` 文件
4. 生成 `codex_{module}_msgs/` 包的 `CMakeLists.txt`
5. 生成 `codex_{module}_msgs/` 包的 `package.xml`
6. 为每个接口类型提供 C++ 使用示例（Publisher/Subscriber/Client/Server）

### Step 3：创建文件

在 `codex_{module}_msgs/` 目录下按标准结构创建所有文件：
```
codex_{module}_msgs/
├── msg/
├── srv/
├── action/
├── CMakeLists.txt
└── package.xml
```

### Step 4：验证

检查生成的文件：
- 枚举常量命名规范（全大写）
- Header 字段存在
- srv 文件有 `---` 分隔符
- action 文件有两个 `---` 分隔符

### Step 5：汇报

列出生成的所有文件及其路径。
