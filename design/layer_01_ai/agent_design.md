---

# Agent 模块设计

## 1. 模块概述与定位

**模块名称**：Agent

**定位**：Agent 是端侧软件系统中的 **VLA 具身智能体（Embodied AI）**，位于 AI 层。它是机器人"大脑"的决策核心，负责理解用户意图、融合环境感知、进行推理规划，并生成高层任务计划交由 TE 执行。Agent **不直接控制任何硬件**，所有物理操作通过 TE 调度到底层运动/导航模块。

Agent 参考开源项目 [RoboClaw](https://github.com/MINT-SJTU/RoboClaw) 的 AgentLoop 设计，但适配为 ROS2 分布式节点架构，与端侧系统的 TE、Gateway、Perception 等模块协同工作。

**核心职责**：
1. **自然语言理解**：接收并解析用户的自然语言指令（来自 Gateway 或本地语音）
2. **环境感知融合**：接收 Perception 的视觉/Lidar 感知结果，构建环境认知
3. **推理与规划**：基于 LLM 进行推理，将高层意图拆解为可执行的任务计划
4. **技能调用（Skills）**：通过预定义的技能工具集（perceive、dispatch_task、query_state 等）与系统交互
5. **记忆管理**：维护短期对话上下文和长期任务记忆
6. **任务生成**：向 TE 提交任务计划，监控执行进度，处理失败重规划
7. **用户交互**：必要时向用户确认、追问或报告执行结果

**与相邻模块的边界**：

| 边界 | Agent 负责 | 对方负责 |
|------|-----------|---------|
| Agent ↔ Gateway | 接收 NL 指令，上报响应/结果 | 云端/APP 通信，语音转文字 |
| Agent ↔ TE | 生成任务计划并提交，接收执行反馈 | 任务调度、编排、执行 |
| Agent ↔ Perception | 查询/订阅感知结果 | 视觉+Lidar 感知融合 |
| Agent ↔ SM | 查询机器人状态 | 全局状态机决策 |
| Agent ↔ HDS | 上报决策异常等原始数据 | 故障诊断与定级 |

---

## 2. 职责边界

| 层次 | 模块 | 职责范围 |
|------|------|---------|
| **AI 决策层** | **Agent** | **理解意图、环境认知、推理规划、生成任务（"做什么"+"为什么"）** |
| 任务执行层 | TE | 任务调度、编排、执行监控（"怎么做"+"何时做"） |
| 运动控制层 | MC/MS/MP/PnC | 具体的运动/导航指令执行（"做动作"） |
| 状态管理层 | SM | 全局状态机，运动许可校验 |

**Agent 不做的事情**（红线）：

- **不直接控制关节/运动** — 所有运动指令通过 TE 调度，Agent 只生成任务描述
- **不直接操作进程** — 不调用 subprocess、systemctl；进程启停通过 EM 接口（间接）
- **不直接连接云端** — 所有云端通信通过 Gateway 转发
- **不做故障定级** — 只上报原始决策数据给 HDS，由 HDS 定级
- **不做底层感知算法** — 视觉/Lidar 融合由 Perception 负责，Agent 只消费感知结果
- **不做实时控制** — Agent 推理是秒级（LLM 几百毫秒~几秒），不参与 1kHz 控制循环

---

## 3. 内部架构

### 3.1 Agent 核心循环（参考 RoboClaw AgentLoop）

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         AgentNode (ROS2)                                │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                     Agent Loop (异步)                           │   │
│  │                                                                 │   │
│  │  1. Receive: 接收消息（NL指令 / 感知更新 / 任务反馈）            │   │
│  │  2. Context Build: 构建上下文（历史 + 记忆 + 技能 + 环境状态）   │   │
│  │  3. LLM Inference: 调用 LLM（本地/云端）→ 生成响应/计划         │   │
│  │  4. Tool Execution: 解析并执行工具调用                          │   │
│  │  5. Response: 发送响应给用户 / 提交任务给 TE                    │   │
│  │                                                                 │   │
│  │  max_iterations = 20（防止无限循环）                            │   │
│  │  context_window = 128k tokens                                   │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐   │
│  │  Skill Registry │   │  Memory Manager │   │  Context Builder│   │
│  │  (技能注册表)   │   │  (记忆管理)     │   │  (上下文构建)   │   │
│  │                 │   │                 │   │                 │   │
│  │ - perceive      │   │ - 短期对话记忆  │   │ - 系统提示词    │   │
│  │ - query_state   │   │ - 长期任务记忆  │   │ - 历史消息      │   │
│  │ - dispatch_task │   │ - 自动归档      │   │ - 工具定义      │   │
│  │ - cancel_task   │   │                 │   │ - 环境快照      │   │
│  │ - ask_user      │   │                 │   │                 │   │
│  └─────────────────┘   └─────────────────┘   └─────────────────┘   │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                   ROS2 Interface Layer                          │   │
│  │                                                                 │   │
│  │  Subscribers: /gateway/user_cmd, /perception/fusion_result, ...│   │
│  │  Publishers: /agent/agent_response, /agent/task_proposal, ...  │   │
│  │  Service Server: /agent/query_status                           │   │
│  │  Action Server: /agent/execute_interaction                     │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.2 核心组件

#### 3.2.1 Skill Registry（技能注册表）

Agent 通过**技能（Skills）**与端侧系统交互。每个技能是一个预定义的工具函数，有明确的输入/输出 schema。

**内置技能集**：

| 技能名 | 功能 | 调用目标 | 示例 |
|--------|------|---------|------|
| `perceive` | 查询当前环境感知结果 | Perception | "看看前面有什么" |
| `query_state` | 查询机器人当前状态 | SM / MC / TE | "机器人现在在哪" |
| `dispatch_task` | 向 TE 提交任务计划 | TE | "走到门口蹲下" |
| `cancel_task` | 取消正在执行的任务 | TE | "停下来" |
| `ask_user` | 向用户发起确认/追问 | Gateway | "需要我帮您拿哪个杯子？" |
| `query_map` | 查询地图信息 | MapManager | "最近的出口在哪" |
| `navigate_to` | 导航到指定位置（封装 dispatch_task）| TE → PnC | "去厨房" |
| `execute_motion` | 执行特定动作（封装 dispatch_task）| TE → MC/MP | "挥挥手" |

**技能定义示例（JSON Schema）**：

```json
{
  "name": "dispatch_task",
  "description": "向任务引擎提交一个任务计划",
  "parameters": {
    "type": "object",
    "properties": {
      "task_type": {
        "type": "string",
        "enum": ["MOTION", "NAVIGATION", "TELEOP", "RECORD", "REPLAY", "INFER", "CALIBRATE", "COMPOSITE"]
      },
      "description": { "type": "string", "description": "任务的自然语言描述" },
      "parameters": { "type": "object", "description": "任务参数（模块特定的JSON）" },
      "priority": { "type": "integer", "default": 50 }
    },
    "required": ["task_type", "description"]
  }
}
```

#### 3.2.2 Memory Manager（记忆管理器）

参考 RoboClaw 的 `MemoryConsolidator`，维护两层记忆：

- **短期记忆（Session Memory）**：当前对话/任务的上下文，保留最近 N 轮交互
- **长期记忆（Long-term Memory）**：
  - 任务执行历史（成功/失败模式）
  - 用户偏好（如常用指令、习惯）
  - 环境认知（房间布局、常用物品位置）

**记忆自动归档**：当短期记忆超过 token 阈值（如 32k）时，自动提取关键信息归档到长期记忆。

#### 3.2.3 Context Builder（上下文构建器）

每次 LLM 调用前，构建完整的上下文消息：

```
System Prompt（角色定义 + 安全约束 + 可用技能列表）
  ↓
Long-term Memory（相关的历史任务/偏好）
  ↓
Session History（当前对话的最近 N 轮）
  ↓
Environment Snapshot（当前机器人状态、感知结果摘要）
  ↓
Current Message（用户当前指令）
```

**环境快照（Environment Snapshot）**包含：
- 当前 SM 状态（STAND/WALKING 等）
- 当前位置（SLAM 定位）
- 最近感知结果摘要（障碍物、人物、物品）
- 正在执行的任务状态

---

## 4. ROS2 接口定义

### 4.1 消息定义（msg）

```
# agent_msgs/msg/AgentResponse.msg
# Agent 响应消息

string response_id               # 响应唯一 ID
string content                   # 响应内容（自然语言）
string[] skills_used             # 本次响应用的技能列表
string task_id                   # 关联的任务 ID（如提交了任务）
float32 confidence               # Agent 对响应的置信度 0.0~1.0
builtin_interfaces/Time stamp
```

```
# agent_msgs/msg/TaskProposal.msg
# Agent 向 TE 提交的任务提案

string proposal_id               # 提案 ID
string description               # 任务自然语言描述
uint8 task_type                  # 任务类型（对应 TE 枚举）
string parameters_json           # 任务参数 JSON
int32 priority                   # 优先级
string reason                    # Agent 生成此任务的原因
builtin_interfaces/Time stamp
```

```
# agent_msgs/msg/AgentState.msg
# Agent 状态广播

uint8 AGENT_IDLE       = 0
uint8 AGENT_THINKING   = 1   # 正在推理
uint8 AGENT_EXECUTING  = 2   # 正在执行技能
uint8 AGENT_WAITING    = 3   # 等待用户确认/等待任务完成
uint8 AGENT_ERROR      = 4   # 出错

uint8 state
string current_skill             # 当前正在执行的技能
string current_task_id           # 当前关联的任务
float32 progress_percent         # 0.0~100.0
builtin_interfaces/Time stamp
```

```
# agent_msgs/msg/Heartbeat.msg
# Agent 心跳

builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message
bool llm_healthy                 # LLM 推理服务是否正常
```

```
# agent_msgs/msg/ErrorCode.msg
# Agent 错误码（注：应命名为 ErrorCode.msg，非 AgentErrorCode.msg）

uint16 OK                        = 0
uint16 ERR_LLM_UNAVAILABLE       = 5001   # LLM 推理服务不可用
uint16 ERR_LLM_TIMEOUT           = 5002   # LLM 推理超时
uint16 ERR_INVALID_SKILL_CALL    = 5003   # 技能调用参数无效
uint16 ERR_SKILL_EXECUTION_FAIL  = 5004   # 技能执行失败
uint16 ERR_TE_REJECTED           = 5005   # TE 拒绝任务提案
uint16 ERR_PERCEPTION_TIMEOUT    = 5006   # 感知查询超时
uint16 ERR_MAX_ITERATIONS        = 5007   # 达到最大迭代次数
uint16 ERR_CONTEXT_OVERFLOW      = 5008   # 上下文超出 token 限制
uint16 ERR_MEMORY_FAIL           = 5009   # 记忆读写失败

uint16 error_code
string message
```

### 4.2 服务定义（srv）

```
# agent_msgs/srv/QueryStatus.srv
# 查询 Agent 状态

# Request（空）
---
agent_msgs/AgentState state
bool llm_connected
uint32 session_token_count       # 当前会话 token 数
```

```
# agent_msgs/srv/ClearMemory.srv
# 清除会话记忆（用户说"忘掉刚才的对话"）

bool clear_long_term             # 是否同时清除长期记忆
---
bool success
string message
```

```
# agent_msgs/srv/GetHealthStatus.srv
# 健康状态查询

# Request（空）
---
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
uint32 total_interactions        # 累计交互次数
uint32 total_tasks_proposed      # 累计提交任务数
```

### 4.3 Action 定义（action）

```
# agent_msgs/action/ExecuteInteraction.action
# 执行交互（TE 调用 Agent 完成交互子任务）

# Goal
string user_message              # 用户消息/指令
string context_json              # 上下文信息 JSON
---
# Result
bool success
string response                  # Agent 响应内容
uint16 error_code
---
# Feedback
string current_phase             # 当前阶段：thinking / tool_call / responding
string current_skill             # 当前调用的技能
```

### 4.4 接口汇总表

#### Topics（发布）

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/agent/agent_response` | `agent_msgs/msg/AgentResponse` | Agent → Gateway | Reliable + Volatile, Depth 10 | 事件驱动 | Agent 响应广播 |
| `/agent/task_proposal` | `agent_msgs/msg/TaskProposal` | Agent → TE | Reliable + Volatile, Depth 10 | 事件驱动 | 任务提案 |
| `/agent/agent_state` | `agent_msgs/msg/AgentState` | Agent → ALL | Reliable + Volatile, Depth 1 | 事件驱动 | Agent 状态 |
| `/agent/heartbeat` | `agent_msgs/msg/Heartbeat` | Agent → EM/HDS | Reliable + Volatile, Depth 1 | 1Hz | 心跳 |

#### Topics（订阅）

| 名称 | 类型 | 来源 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/gateway/user_command` | `std_msgs/msg/String` | Gateway | Reliable + Volatile | 事件驱动 | 用户自然语言指令 |
| `/perception/fusion_result` | `perception_msgs/msg/FusionResult` | Perception | Best Effort, Depth 1 | 10Hz | 感知融合结果 |
| `/sm/robot_state` | `sm_msgs/msg/RobotState` | SM | Reliable + Transient Local | 事件驱动 | 全局状态 |
| `/te/task_state` | `te_msgs/msg/TaskState` | TE | Reliable + Volatile | 事件驱动 | 任务执行反馈 |
| `/pnc/pnc_state` | `pnc_msgs/msg/PncState` | PnC | Reliable + Volatile | 10Hz | 导航状态（可选）|
| `/mc/mc_state` | `mc_msgs/msg/McState` | MC | Reliable + Volatile | 事件驱动 | 运动模式状态（可选）|

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/agent/query_status` | `agent_msgs/srv/QueryStatus` | Gateway / TE | 查询 Agent 状态 |
| `/agent/clear_memory` | `agent_msgs/srv/ClearMemory` | Gateway | 清除记忆 |
| `/agent/get_health_status` | `agent_msgs/srv/GetHealthStatus` | EM / HDS | 健康查询 |

#### Actions

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/agent/execute_interaction` | `agent_msgs/action/ExecuteInteraction` | TE | TE 调用 Agent 执行交互子任务 |

---

## 5. 内部设计

### 5.1 节点架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           AgentNode                                     │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                        Agent Core                                │   │
│  │                                                                  │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │   │
│  │  │  Message    │  │  Context    │  │  LLM Provider           │  │   │
│  │  │  Router     │──►│  Builder    │──►│ (本地/云端)             │  │   │
│  │  │             │  │             │  │                         │  │   │
│  │  │ - 分类消息  │  │ - 拼接上下文│  │ - 本地 LLM（端侧 NPU） │  │   │
│  │  │ - 路由处理  │  │ - 注入技能  │  │ - 云端 API（备用）      │  │   │
│  │  └─────────────┘  └─────────────┘  └─────────────────────────┘  │   │
│  │           │                                    │                │   │
│  │           ▼                                    ▼                │   │
│  │  ┌──────────────────────────────────────────────────────────┐  │   │
│  │  │                    Tool Executor                         │  │   │
│  │  │  - 解析 LLM 输出的工具调用                               │  │   │
│  │  │  - 调用 Skill Registry 中对应的技能                      │  │   │
│  │  │  - 收集工具执行结果                                      │  │   │
│  │  │  - 返回给 LLM 继续推理                                   │  │   │
│  │  └──────────────────────────────────────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐   │
│  │  Skill Registry │   │  Memory Manager │   │  Config Manager │   │
│  │  (技能注册表)   │   │  (记忆管理)     │   │  (参数管理)     │   │
│  └─────────────────┘   └─────────────────┘   └─────────────────┘   │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                   ROS2 Interface Layer                          │   │
│  │                                                                  │   │
│  │  Subscribers: user_cmd, fusion_result, robot_state, ...        │   │
│  │  Publishers: agent_response, task_proposal, agent_state, ...   │   │
│  │  Service Server: query_status, clear_memory                    │   │
│  │  Action Server: execute_interaction                            │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.2 关键设计决策

1. **LLM 推理可切换**：支持本地模型（端侧 NPU，延迟 < 500ms）和云端 API（能力更强，延迟 1-3s）两种模式，可配置切换
2. **纯异步架构**：Agent 的所有操作都是异步的，不阻塞 ROS2 回调线程
3. **技能幂等性**：所有技能设计为幂等操作，重复调用不会导致副作用
4. **任务提案确认**：Agent 向 TE 提交任务前，先通过 `/agent/task_proposal` Topic 广播提案，TE 决定是否接受（TE 有最终调度权）
5. **上下文自动压缩**：当上下文接近 token 限制时，自动压缩历史消息（保留关键信息，删除冗余内容）

### 5.3 关键流程

#### 5.3.1 用户指令处理流程

```
Gateway 发布 /gateway/user_command ("帮我把红色杯子放到桌上")
  → Agent 接收消息
  → Message Router 分类：用户指令 → 进入 Agent Loop
  → Context Builder 构建上下文：
      - 系统提示词（角色、约束、可用技能）
      - 长期记忆（用户偏好、房间布局）
      - 会话历史（最近 10 轮）
      - 环境快照（当前状态、位置、最近感知）
  → LLM 推理（第 1 轮）：
      - LLM 决定调用 perceive 技能
  → Tool Executor 执行 perceive：
      - 调用 Perception Service 查询红色杯子位置
      - 返回杯子 3D 坐标
  → LLM 推理（第 2 轮）：
      - LLM 决定调用 dispatch_task 技能
      - 生成任务计划：
        1. 导航到桌子旁（TE_TYPE_NAVIGATION）
        2. 抓取杯子（TE_TYPE_MOTION）
        3. 放到桌上（TE_TYPE_MOTION）
  → Tool Executor 执行 dispatch_task：
      - 发布 /agent/task_proposal（复合任务）
      - TE 接收并调度执行
  → LLM 推理（第 3 轮）：
      - LLM 生成自然语言响应："好的，我这就去把红色杯子放到桌上"
  → 发布 /agent/agent_response
```

#### 5.3.2 任务执行监控流程

```
TE 开始执行 Agent 提交的任务
  → Agent 订阅 /te/task_state
  → 任务进度更新时：
      - 如果进度正常 → 不打扰用户
      - 如果需要用户确认 → 调用 ask_user 技能
      - 如果任务失败 → LLM 重新规划（重试/换方案/报告用户）
  → 任务完成：
      - 发布 /agent/agent_response（"已经放好了"）
      - 更新长期记忆（任务成功模式）
```

#### 5.3.3 E-Stop / FAULT 时的 Agent 行为

```
SM 状态变为 ACTIVE_E_STOP 或 FAULT
  → Agent 订阅收到
  → 立即中止当前 Agent Loop（如果有正在进行的推理）
  → 停止提交新任务
  → 向用户发送响应："检测到急停/故障，已停止当前操作"
  → 等待状态恢复后，询问用户是否继续
```

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|------|------|------|------|
| Gateway | Gateway → Agent | `/gateway/user_command` (Topic) | 用户自然语言指令 |
| Gateway | Agent → Gateway | `/agent/agent_response` (Topic) | Agent 响应 |
| TE | Agent → TE | `/agent/task_proposal` (Topic) | 任务提案 |
| TE | TE → Agent | `/te/task_state` (Topic) | 任务执行反馈 |
| TE | TE → Agent | `/agent/execute_interaction` (Action) | TE 调用交互子任务 |
| Perception | Perception → Agent | `/perception/fusion_result` (Topic) | 感知结果 |
| Perception | Agent → Perception | `/perception/query` (Service) | 主动查询感知 |
| SM | SM → Agent | `/sm/robot_state` (Topic) | 全局状态 |
| HDS | Agent → HDS | `/hds/health_report` (Topic) | 上报异常 |
| HDS | Agent → HDS | `/agent/heartbeat` (Topic) | 心跳 |
| DR | Agent → DR | `/agent/agent_state` (Topic) | Agent 决策记录 |
| MC | MC → Agent | `/mc/mc_state` (Topic) | 运动模式状态（可选订阅）|
| PnC | PnC → Agent | `/pnc/pnc_state` (Topic) | 导航状态（可选订阅）|

### 6.2 与 TE 的协作边界

| 场景 | Agent 做什么 | TE 做什么 |
|------|-------------|----------|
| 用户说"去厨房" | 理解意图 → 查询地图 → 生成导航任务提案 | 接收任务 → 调用 PnC → 监控执行 |
| 用户说"停下" | 理解意图 → 生成 cancel_task 调用 | 接收取消 → 停止当前任务 |
| 任务失败 | 分析原因 → 决定重试/换方案/问用户 | 执行重试或新任务 |
| 多步骤任务 | 拆解为子任务序列，逐个提案 | 按依赖调度子任务 |

---

## 7. 关键参数与配置

```yaml
# agent/config/agent_params.yaml

agent:
  ros__parameters:
    # LLM 配置
    llm:
      provider: "local"             # local / cloud
      local_model_path: "/opt/striding/models/agent_llm.gguf"
      cloud_api_endpoint: ""         # 云端 API 地址
      cloud_api_key: ""              # API Key
      max_tokens: 4096
      temperature: 0.7
      timeout_sec: 10.0

    # 推理模式切换阈值
    auto_switch_to_cloud: false     # 本地失败时是否自动切云端

    # Agent Loop 配置
    max_iterations: 20              # 单次用户指令最大迭代次数
    context_window_tokens: 32768    # 上下文窗口 token 限制

    # 记忆配置
    memory:
      short_term_max_rounds: 20     # 短期记忆保留轮数
      long_term_enabled: true       # 是否启用长期记忆
      auto_consolidate_threshold: 0.8  # 记忆自动归档阈值（token 占比）

    # 技能配置
    skills:
      builtin_dir: "/opt/striding/agent/skills"
      user_dir: "~/.roboclaw/agent/skills"
      timeout_sec: 30.0             # 技能执行超时

    # 环境快照更新频率
    env_snapshot_rate_hz: 1.0

    # 心跳频率
    heartbeat_rate_hz: 1.0

    # 调试
    verbose_llm_log: false          # 是否记录 LLM 输入输出
    publish_thinking_process: false # 是否发布推理过程（调试用）
```

---

## 8. 错误码定义

| 错误码 | 常量名 | 说明 | 严重级别 |
|--------|--------|------|---------|
| 0 | `OK` | 成功 | — |
| 5001 | `ERR_LLM_UNAVAILABLE` | LLM 推理服务不可用（本地模型未加载/云端连接失败）| HIGH |
| 5002 | `ERR_LLM_TIMEOUT` | LLM 推理超时（> timeout_sec）| MEDIUM |
| 5003 | `ERR_INVALID_SKILL_CALL` | 技能调用参数不符合 schema | MEDIUM |
| 5004 | `ERR_SKILL_EXECUTION_FAIL` | 技能执行失败（如 Perception 查询失败）| MEDIUM |
| 5005 | `ERR_TE_REJECTED` | TE 拒绝任务提案（如 SM 状态不允许）| MEDIUM |
| 5006 | `ERR_PERCEPTION_TIMEOUT` | 感知查询超时 | MEDIUM |
| 5007 | `ERR_MAX_ITERATIONS` | 达到最大迭代次数（任务未完成）| MEDIUM |
| 5008 | `ERR_CONTEXT_OVERFLOW` | 上下文超出 token 限制，无法继续推理 | MEDIUM |
| 5009 | `ERR_MEMORY_FAIL` | 记忆读写失败（存储故障）| LOW |

---

## 9. 安全约束

### 9.1 不直接控制硬件

- Agent **禁止**直接发布关节控制指令
- Agent **禁止**直接调用 HAL_EtherCAT 的 Service
- Agent 只能通过 `dispatch_task` 技能向 TE 提交任务，由 TE 调度执行

### 9.2 任务提案校验

- Agent 提交的任务提案中，必须包含 `reason` 字段（Agent 生成此任务的原因）
- TE 有权拒绝 Agent 的任务提案（如 SM 状态不允许、任务参数无效）
- Agent 收到拒绝后，应尝试重规划或向用户说明原因

### 9.3 危险指令过滤

- Agent 内置危险指令检测（如"撞墙"、"从楼梯跳下去"）
- 检测到危险指令时，拒绝执行并向用户说明安全风险

### 9.4 隐私保护

- 用户对话内容不直接上传云端（除非配置为 cloud 模式）
- 本地 LLM 模式下，所有推理在端侧完成

---

## 10. 包结构

```
agent_msgs/
├── msg/
│   ├── AgentResponse.msg         # Agent 响应
│   ├── TaskProposal.msg          # 任务提案
│   ├── AgentState.msg            # Agent 状态
│   ├── Heartbeat.msg             # 心跳
│   └── ErrorCode.msg             # 错误码（标准命名）
├── srv/
│   ├── QueryStatus.srv           # 查询状态
│   ├── ClearMemory.srv           # 清除记忆
│   └── GetHealthStatus.srv       # 健康查询
├── action/
│   └── ExecuteInteraction.action # 执行交互
├── CMakeLists.txt
└── package.xml

agent/
├── include/agent/
│   ├── agent_node.hpp            # 主节点类
│   ├── agent_loop.hpp            # Agent 核心循环
│   ├── skill_registry.hpp        # 技能注册表
│   ├── skill_base.hpp            # 技能基类
│   ├── memory_manager.hpp        # 记忆管理器
│   ├── context_builder.hpp       # 上下文构建器
│   ├── llm_provider.hpp          # LLM 提供者（本地/云端）
│   ├── tool_executor.hpp         # 工具执行器
│   └── message_router.hpp        # 消息路由器
├── src/
│   ├── agent_node.cpp
│   ├── agent_loop.cpp
│   ├── skill_registry.cpp
│   ├── memory_manager.cpp
│   ├── context_builder.cpp
│   ├── llm_provider.cpp
│   ├── tool_executor.cpp
│   └── message_router.cpp
├── skills/                       # 内置技能实现
│   ├── perceive_skill.py
│   ├── query_state_skill.py
│   ├── dispatch_task_skill.py
│   ├── cancel_task_skill.py
│   ├── ask_user_skill.py
│   └── query_map_skill.py
├── test/
│   ├── test_agent_loop.cpp
│   ├── test_skill_registry.cpp
│   ├── test_memory_manager.cpp
│   ├── test_context_builder.cpp
│   └── test_integration.cpp
├── config/
│   └── agent_params.yaml         # 参数配置
├── launch/
│   └── agent.launch.py
├── models/                       # 本地 LLM 模型（部署时复制）
│   └── agent_llm.gguf
├── CMakeLists.txt
└── package.xml
```

---

## 11. 关键性能指标（KPI）

| 指标 | 目标值 |
|------|--------|
| LLM 推理延迟（本地） | < 500ms |
| LLM 推理延迟（云端） | < 3s |
| 单次用户指令处理时间 | < 10s（含多轮工具调用）|
| 技能调用延迟 | < 100ms（本地 Service）|
| 任务提案生成延迟 | < 2s |
| 上下文构建时间 | < 50ms |
| 心跳抖动 | < 50ms |
| Agent 自身 CPU 占用 | < 10%（推理时）/< 1%（空闲时）|
| Agent 自身内存占用 | < 2GB（含本地 LLM 模型）|
| 本地 LLM 模型大小 | < 8GB（量化后）|

---

## 附录：与 RoboClaw 的差异

| 维度 | RoboClaw | 本端侧 Agent |
|------|----------|-------------|
| 架构 | 单体 Python 应用 | ROS2 分布式节点 |
| Agent→执行 | Agent 直接调用 embodied 工具 | Agent 通过 TE 调度 |
| 通信 | 内部 bus/queue | ROS2 Topic/Service/Action |
| 实时性 | 软实时（子进程调用）| 不参与实时控制，秒级推理 |
| 安全链 | Agent 直接操作硬件 | Agent 不触碰硬件，TE+SM 把关 |
| LLM | 单一云端 API | 本地+云端可切换 |
| 记忆 | 文件持久化 | ROS2 参数 + 本地存储 |
