---
name: module-design-template
description: 模块设计文档模板，包含所有必需章节的结构和示例。设计新模块时自动加载提供参考。
user-invocable: false
---

# 模块设计文档模板

## 文档结构（必须包含以下所有章节）

```markdown
# {模块全称}（{缩写}）模块设计

## 1. 模块概述

**定位**：{一句话定位，说明在系统分层中的位置}

**核心职责**：
- {职责1}
- {职责2}
- {职责3}（3-5条）

## 2. 职责边界

| 层次 | 模块 | 职责范围 |
|------|------|---------|
| 上层 | {上层模块} | {上层职责} |
| **本模块** | **{本模块}** | **{本模块职责}** |
| 下层 | {下层模块} | {下层职责} |

## 3. 状态机设计（如适用）

### 3.1 状态定义

| 状态 | 枚举值 | 说明 |
|------|--------|------|
| STATE_A | 0 | 说明 |

### 3.2 状态转换表

| 当前状态 | 目标状态 | 触发条件 | 发起方 |
|----------|----------|----------|--------|

## 4. ROS2 接口定义

### 4.1 消息类型

#### XxxState.msg
\`\`\`
# 文件路径：{module}_msgs/msg/XxxState.msg
# 枚举常量
uint8 STATE_A = 0

std_msgs/Header header
uint8 state
string reason
\`\`\`

### 4.2 服务类型

#### DoXxx.srv
\`\`\`
# 文件路径：{module}_msgs/srv/DoXxx.srv
string requester
uint8 target
---
bool accepted
string reason
\`\`\`

### 4.3 Action 类型（如适用）

#### XxxTo.action
\`\`\`
# Goal
uint8 target
int32 timeout_sec
---
# Result
bool success
string message
---
# Feedback
float32 progress
string phase
\`\`\`

### 4.4 接口汇总

**Topic**

| Topic 名称 | 类型 | QoS | 说明 |
|------------|------|-----|------|

**Service**

| Service 名称 | 类型 | 说明 |
|--------------|------|------|

**Action**

| Action 名称 | 类型 | 说明 |
|-------------|------|------|

## 5. 节点内部设计

### 5.1 节点结构

\`\`\`
{module}_node
├── ComponentA    # 说明
│   └── method()
└── ComponentB
\`\`\`

### 5.2 关键流程

（启动流程、主业务流程、故障处理流程）

## 6. 与其他模块的交互

| 模块 | 交互方向 | 接口 | 说明 |
|------|----------|------|------|
| SM | {模块} → SM | /sm/transition_request | 说明 |

## 7. 关键参数与配置

\`\`\`yaml
{module}:
  ros__parameters:
    param_name: default_value  # 说明
\`\`\`

## 8. 错误码定义

\`\`\`
# 文件路径：{module}_msgs/msg/ErrorCode.msg
uint32 OK                    = 0
uint32 ERROR_TYPE_A          = 1001  # 说明
\`\`\`

## 9. 包结构

\`\`\`
{module}_msgs/
├── msg/
├── srv/
├── action/
├── CMakeLists.txt
└── package.xml

{module}/
├── include/{module}/
├── src/
├── config/
│   └── {module}_params.yaml
└── launch/
    └── {module}.launch.py
\`\`\`
```

## 质量检查清单

设计文档完成后，确认：
- [ ] 每个 Topic 都有 QoS 说明
- [ ] 每个 Service response 都有 `bool accepted/success` + `string message/reason`
- [ ] 涉及运动控制的接口有 `priority` 字段
- [ ] 有心跳上报接口（Topic 或定时 Service）
- [ ] 有健康状态查询接口（Service）
- [ ] 错误码从 1001 开始，按功能分段（1xxx/2xxx/3xxx）
- [ ] 参数文件有默认值和注释
