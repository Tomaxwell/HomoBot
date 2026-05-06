---

# Task Engine 模块设计

## 1. 模块概述与定位

**模块名称**：Task Engine（TE）

**定位**：TE 是端侧软件系统中的**任务调度与执行中枢**，位于中间件层与 AI 层之间。它是机器人业务意图（来自 Gateway / Agent）与底层运动/规划执行（MC/MS/MP/PnC）之间的桥梁，负责任务的全生命周期管理——从任务接收、解析、调度、执行到完成/取消/失败处理。TE 是系统中唯一有权将机器人从 `ACTIVE_IDLE` 推进到 `ACTIVE_BUSY` 状态的模块（通过 SM 状态转换请求），也是任务执行期间所有子操作的原子性协调者。

**核心职责**：

1. **任务接收与解析**：接收来自 Gateway（云端/APP）和 Agent（自主决策）的任务请求，解析任务类型、参数和依赖约束
2. **任务调度与编排**：维护任务队列，按优先级和依赖关系调度任务；支持顺序执行、并行子任务、条件分支等编排模式
3. **任务生命周期管理**：管理任务从 `CREATED` → `SCHEDULED` → `RUNNING` → `{COMPLETED|CANCELLED|FAILED}` 的完整状态流转
4. **运动安全协调**：任务执行前通过 SM 状态校验，任务开始时请求 `ACTIVE_IDLE → ACTIVE_BUSY`，任务结束后请求 `ACTIVE_BUSY → ACTIVE_IDLE`
5. **子任务分发**：将任务拆解为子操作，调用 MC/MS/MP（动作/运动）、PnC（导航）、Agent（交互）等模块的 Action/Service 接口
6. **执行监控与上报**：监控任务执行进度，向 Gateway 同步任务状态，向 HDS 上报执行健康数据

**与相邻模块的边界**：

| 边界 | TE 负责 | 对方负责 |
|------|---------|---------|
| TE ↔ Gateway | 接收任务请求，上报任务状态/进度 | 云端/APP 通信，用户指令转发 |
| TE ↔ Agent | 接收自主决策任务，反馈执行结果 | 环境理解、意图决策、任务生成 |
| TE ↔ SM | 请求状态转换（ACTIVE_IDLE ↔ ACTIVE_BUSY），查询运动许可 | 全局状态机决策，状态转换仲裁 |
| TE ↔ MC/MS/MP | 下发动作/运动子任务（通过 Action），接收执行反馈 | 关节级运动控制，动作播放 |
| TE ↔ PnC | 下发导航子任务（通过 Action），接收导航反馈 | 路径规划与自主导航 |
| TE ↔ HDS | 上报任务执行异常、超时等原始数据 | 故障诊断与定级 |
| TE ↔ EM | 请求启动/停止任务依赖的模块进程（间接，通过 EM Service） | 进程生命周期治理 |

---

## 2. 职责边界

| 层次 | 模块 | 职责范围 |
|------|------|---------|
| AI 决策层 | Agent | 环境感知、意图理解、任务生成（"做什么"） |
| **任务执行层** | **Task Engine（TE）** | **任务调度、编排、执行、生命周期管理（"怎么做"）** |
| 运动控制层 | MC/MS/MP/PnC | 具体的运动/导航指令执行（"做动作"） |
| 状态管理层 | SM | 全局状态机，运动许可校验 |

**TE 不做的事情**（红线）：

- **不做故障定级** — 只上报原始执行数据给 HDS，由 HDS 定级
- **不直接操作进程** — 不调用 `subprocess`、`systemctl`、`kill`；进程启停通过 EM 接口
- **不直接连接云端** — 所有云端通信通过 Gateway 转发
- **不做关节级运动控制** — 运动指令通过 MC/MS/MP/PnC 的 Action 接口下发
- **不做全局状态机决策** — 状态转换请求由 SM 仲裁，TE 只发起请求

---

## 3. 状态机设计

TE 维护两层状态机：**任务级状态机**（每个任务独立）和 **调度器状态机**（TE 自身运行状态）。

### 3.1 任务级状态机

#### 3.1.1 状态枚举

| 状态 | 值 | 说明 |
|------|-----|------|
| `TE_TASK_CREATED` | 0 | 任务已创建，等待调度 |
| `TE_TASK_SCHEDULED` | 1 | 任务已入队，等待前置条件满足 |
| `TE_TASK_PREPARING` | 2 | 正在准备执行（SM 状态转换、模块就绪检查） |
| `TE_TASK_RUNNING` | 3 | 任务正在执行 |
| `TE_TASK_PAUSED` | 4 | 任务被暂停（可恢复） |
| `TE_TASK_CANCELLING` | 5 | 正在取消（等待子任务清理） |
| `TE_TASK_COMPLETED` | 6 | 任务正常完成 |
| `TE_TASK_FAILED` | 7 | 任务执行失败 |
| `TE_TASK_CANCELLED` | 8 | 任务已被取消 |

#### 3.1.2 状态转换图

```
                    ┌─────────────────────────────────────────┐
                    │                                         │
              ┌─────┴─────┐  preconditions_met    ┌───────────┴───┐
   create────►│  CREATED  │──────────────────────►│   SCHEDULED   │
              └───────────┘                       └───────┬───────┘
                    │                                     │
                    │ invalid_params                      │ dependencies_ready
                    ▼                                     ▼
              ┌───────────┐                       ┌───────────┐
              │  FAILED   │◄──────────────────────┤ PREPARING │
              └─────┬─────┘   prepare_timeout     └─────┬─────┘
                    │                                   │
                    │                                   │ sm_ready
                    │                                   ▼
                    │                             ┌───────────┐
                    │         pause               │  RUNNING  │
                    │◄────────────────────────────┤           │
                    │                             └─────┬─────┘
                    │                                   │
                    │         resume                    │ execute_complete
                    ├───────────────────────────────────┘
                    │
                    │    cancel_request    ┌───────────┐
                    ├─────────────────────►│CANCELLING │
                    │                      └─────┬─────┘
                    │                            │ cleanup_done
                    │                            ▼
                    │                      ┌───────────┐
                    └──────────────────────┤ CANCELLED │
                                           └───────────┘

              RUNNING ──execute_error──► FAILED
              RUNNING ──execute_success──► COMPLETED
              PREPARING ──sm_reject──► FAILED
              SCHEDULED ──timeout──► FAILED
```

#### 3.1.3 状态转换表

| 当前状态 | 目标状态 | 触发条件 | 发起方 | 说明 |
|----------|----------|----------|--------|------|
| `CREATED` | `SCHEDULED` | 参数校验通过，前置条件满足 | TE 内部调度器 | 任务入队 |
| `CREATED` | `FAILED` | 参数校验失败 | TE 内部 | 立即失败，不上报 SM |
| `SCHEDULED` | `PREPARING` | 依赖任务完成 / 无依赖 | TE 调度器 | 开始执行准备 |
| `SCHEDULED` | `FAILED` | 调度超时（默认 60s） | TE 内部 | 前置条件长期不满足 |
| `PREPARING` | `RUNNING` | SM 状态转换成功（ACTIVE_BUSY） | TE 内部 | 开始执行子任务 |
| `PREPARING` | `FAILED` | SM 拒绝转换 / 准备超时（10s） | SM / TE | 无法进入执行状态 |
| `RUNNING` | `PAUSED` | 收到暂停请求 | Gateway / Agent | 暂停当前子任务 |
| `RUNNING` | `CANCELLING` | 收到取消请求 | Gateway / Agent / SM(E-Stop) | 发起取消流程 |
| `RUNNING` | `COMPLETED` | 所有子任务成功完成 | TE 内部 | 正常结束 |
| `RUNNING` | `FAILED` | 子任务失败 / 执行超时 | TE 内部 / 子模块 | 执行失败 |
| `PAUSED` | `RUNNING` | 收到恢复请求 | Gateway / Agent | 恢复执行 |
| `PAUSED` | `CANCELLING` | 收到取消请求 | Gateway / Agent | 取消暂停中的任务 |
| `CANCELLING` | `CANCELLED` | 子任务清理完成 | TE 内部 | 取消成功 |
| `CANCELLING` | `FAILED` | 取消过程中发生错误 | TE 内部 | 取消失败，按失败处理 |

### 3.2 调度器状态机（TE 自身运行状态）

| 状态 | 值 | 说明 |
|------|-----|------|
| `TE_SCHEDULER_INIT` | 0 | TE 初始化中 |
| `TE_SCHEDULER_IDLE` | 1 | 空闲，无任务执行 |
| `TE_SCHEDULER_BUSY` | 2 | 正在执行任务 |
| `TE_SCHEDULER_PAUSED` | 3 | 调度器被暂停（不调度新任务） |
| `TE_SCHEDULER_DEGRADED` | 4 | 降级模式（部分模块不可用，只执行受限任务） |
| `TE_SCHEDULER_FAULT` | 5 | 调度器故障，拒绝所有新任务 |

---

## 4. ROS2 接口定义

### 4.1 消息定义（msg）

```
# te_msgs/msg/TaskState.msg
# 单个任务的状态信息

# 任务状态枚举
uint8 TE_TASK_CREATED     = 0
uint8 TE_TASK_SCHEDULED   = 1
uint8 TE_TASK_PREPARING   = 2
uint8 TE_TASK_RUNNING     = 3
uint8 TE_TASK_PAUSED      = 4
uint8 TE_TASK_CANCELLING  = 5
uint8 TE_TASK_COMPLETED   = 6
uint8 TE_TASK_FAILED      = 7
uint8 TE_TASK_CANCELLED   = 8

# 任务类型枚举
uint8 TE_TYPE_UNKNOWN     = 0
uint8 TE_TYPE_MOTION      = 1    # 运动任务（调用 MP/MC/MS）
uint8 TE_TYPE_NAVIGATION  = 2    # 导航任务（调用 PnC）
uint8 TE_TYPE_INTERACTION = 3    # 交互任务（调用 Agent）
uint8 TE_TYPE_COMPOSITE   = 4    # 复合任务（多子任务编排）
uint8 TE_TYPE_EMERGENCY   = 5    # 紧急任务（最高优先级）
uint8 TE_TYPE_TELEOP     = 6    # 遥操作任务（调用 MC）
uint8 TE_TYPE_RECORD     = 7    # 数据录制任务（调用 DR + MC）
uint8 TE_TYPE_REPLAY     = 8    # 动作回放任务（调用 MP）
uint8 TE_TYPE_INFER      = 9    # VLA 推理任务（调用 MC RL 策略）
uint8 TE_TYPE_CALIBRATE  = 10   # 校准任务（调用 MC）
uint8 TE_TYPE_TRAIN      = 11   # 模型训练任务（后台离线，调用外部资源）

string task_id                     # 任务唯一标识（UUID）
string parent_task_id              # 父任务 ID（子任务时有效，空字符串表示根任务）
uint8 task_type                    # 任务类型
uint8 state                        # 当前任务状态
uint8 previous_state               # 上一个状态
builtin_interfaces/Time created_at # 创建时间
builtin_interfaces/Time started_at # 开始执行时间
builtin_interfaces/Time ended_at   # 结束时间
float32 progress_percent           # 执行进度 0.0~100.0
string current_subtask             # 当前执行的子任务名称
uint16 error_code                  # 失败时的错误码（ErrorCode）
string error_message               # 失败时的错误描述
string requester_node              # 请求者节点名
```

```
# te_msgs/msg/TaskList.msg
# 任务列表（用于状态广播）

TaskState[] tasks                  # 所有任务状态列表
uint32 total_count                 # 总任务数
uint32 running_count               # 运行中任务数
uint32 pending_count               # 等待中任务数
uint32 completed_count             # 已完成任务数
uint32 failed_count                # 失败任务数
builtin_interfaces/Time updated_at # 更新时间
```

```
# te_msgs/msg/SubTask.msg
# 子任务定义（用于复合任务编排）

# 子任务类型枚举
uint8 SUB_TYPE_MOTION_ACTION    = 1   # 调用 MP Action
uint8 SUB_TYPE_NAV_ACTION       = 2   # 调用 PnC Action
uint8 SUB_TYPE_INTERACTION      = 3   # 调用 Agent Service/Action
uint8 SUB_TYPE_WAIT             = 4   # 等待条件
uint8 SUB_TYPE_PARALLEL         = 5   # 并行执行子任务组
uint8 SUB_TYPE_CONDITIONAL      = 6   # 条件分支

string subtask_id                  # 子任务 ID
uint8 subtask_type                 # 子任务类型
string target_module               # 目标模块（mp/pnc/agent）
string action_or_service_name      # 调用的 Action/Service 名称
string parameters_json             # 参数 JSON 字符串
string[] depends_on                # 依赖的子任务 ID 列表
int32 timeout_sec                  # 子任务超时（秒），-1 表示无超时
```

```
# te_msgs/msg/TeSchedulerState.msg
# TE 调度器自身状态广播

uint8 TE_SCHEDULER_INIT       = 0
uint8 TE_SCHEDULER_IDLE       = 1
uint8 TE_SCHEDULER_BUSY       = 2
uint8 TE_SCHEDULER_PAUSED     = 3
uint8 TE_SCHEDULER_DEGRADED   = 4
uint8 TE_SCHEDULER_FAULT      = 5

uint8 state                        # 调度器状态
string current_task_id             # 当前执行中的任务 ID
uint32 queue_depth                 # 任务队列深度
builtin_interfaces/Time state_entered_at  # 进入当前状态的时间
```

```
# te_msgs/msg/Heartbeat.msg
# TE 心跳消息（注：应命名为 Heartbeat.msg，非 TeHeartbeat.msg）

builtin_interfaces/Time stamp
string node_name                   # 节点名称（固定 "task_engine"）
uint8 state                        # 调度器状态
bool healthy
string status_message
uint32 active_task_count           # 活跃任务数
uint32 total_tasks_this_session    # 本次会话累计处理任务数
bool sm_connected                  # SM 连接状态
```

```
# te_msgs/msg/ErrorCode.msg
# TE 错误码定义（注：应命名为 ErrorCode.msg，非 ErrorCode.msg）

uint16 OK                               = 0
uint16 ERR_INVALID_TASK_TYPE            = 4001   # 不支持的任务类型
uint16 ERR_INVALID_PARAMETERS           = 4002   # 任务参数校验失败
uint16 ERR_SM_TRANSITION_REJECTED       = 4003   # SM 状态转换被拒绝
uint16 ERR_MOTION_NOT_ALLOWED           = 4004   # SM 运动许可校验失败
uint16 ERR_SUBTASK_TIMEOUT              = 4005   # 子任务执行超时
uint16 ERR_SUBTASK_FAILED               = 4006   # 子任务执行失败
uint16 ERR_TASK_CANCELLED               = 4007   # 任务被取消
uint16 ERR_DEPENDENCY_NOT_MET           = 4008   # 前置依赖未满足
uint16 ERR_SCHEDULER_PAUSED             = 4009   # 调度器暂停中
uint16 ERR_SCHEDULER_FAULT              = 4010   # 调度器故障
uint16 ERR_MODULE_NOT_READY             = 4011   # 目标模块未就绪
uint16 ERR_ESTOP_ACTIVE                 = 4012   # 急停激活，拒绝任务
uint16 ERR_FAULT_STATE                  = 4013   # FAULT 状态，拒绝任务
uint16 ERR_TASK_NOT_FOUND               = 4014   # 指定任务不存在
uint16 ERR_TASK_NOT_CANCELLABLE         = 4015   # 任务当前状态不可取消
uint16 ERR_CONCURRENT_TASK_LIMIT        = 4016   # 并发任务数达到上限
uint16 ERR_INTERNAL_ERROR               = 4017   # TE 内部错误

uint16 error_code
string message
```

### 4.2 服务定义（srv）

```
# te_msgs/srv/SubmitTask.srv
# 创建并提交一个新任务

# Request
string task_id                     # 任务 ID（空则 TE 自动生成 UUID）
uint8 task_type                    # 任务类型（TaskState 枚举）
string parameters_json             # 任务参数 JSON 字符串
string requester_node              # 请求者节点名
int32 priority                     # 任务优先级（0-100，越大越优先）
builtin_interfaces/Duration max_execution_time  # 最大执行时间
---
# Response
bool accepted                      # 是否接受
ErrorCode error                  # 错误码
string task_id                     # 实际分配的任务 ID
string message                     # 结果描述
```

```
# te_msgs/srv/CancelTask.srv
# 取消指定任务

# Request
string task_id                     # 要取消的任务 ID
string reason                      # 取消原因
string requester_node              # 请求者节点名
---
# Response
bool accepted                      # 是否接受取消请求
ErrorCode error                  # 错误码
string message                     # 结果描述
uint8 task_state                   # 取消后的任务状态
```

```
# te_msgs/srv/PauseTask.srv
# 暂停指定任务

# Request
string task_id                     # 要暂停的任务 ID
string reason                      # 暂停原因
---
# Response
bool accepted                      # 是否接受
ErrorCode error                  # 错误码
string message                     # 结果描述
uint8 task_state                   # 暂停后的任务状态
```

```
# te_msgs/srv/ResumeTask.srv
# 恢复指定任务

# Request
string task_id                     # 要恢复的任务 ID
---
# Response
bool accepted                      # 是否接受
ErrorCode error                  # 错误码
string message                     # 结果描述
uint8 task_state                   # 恢复后的任务状态
```

```
# te_msgs/srv/GetTaskStatus.srv
# 查询任务状态

# Request
string task_id                     # 任务 ID（空字符串表示查询所有任务）
---
# Response
bool success                       # 查询是否成功
ErrorCode error                  # 错误码
TaskState[] tasks                  # 任务状态列表
```

```
# te_msgs/srv/GetHealthStatus.srv
# TE 健康状态查询

# Request（空）
---
# Response
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
uint32 total_tasks_processed       # 累计处理任务数
uint32 total_tasks_failed          # 累计失败任务数
uint32 current_queue_depth         # 当前队列深度
```

```
# te_msgs/srv/ControlScheduler.srv
# 控制调度器（暂停/恢复/清空队列）

# Request
uint8 OP_PAUSE     = 0
uint8 OP_RESUME    = 1
uint8 OP_CLEAR     = 2   # 清空等待队列（不取消正在执行的任务）

uint8 operation                    # 操作类型
string reason                      # 操作原因
---
# Response
bool accepted                      # 是否接受
ErrorCode error                  # 错误码
string message                     # 结果描述
uint8 scheduler_state              # 操作后的调度器状态
```

### 4.3 Action 定义（action）

```
# te_msgs/action/ExecuteTask.action
# 执行任务（长耗时操作，含进度反馈）

# Goal
string task_id                     # 任务 ID
uint8 task_type                    # 任务类型
string parameters_json             # 任务参数
int32 priority                     # 优先级
builtin_interfaces/Duration max_execution_time  # 最大执行时间
---
# Result
bool success                       # 是否成功
ErrorCode error                  # 错误码
string message                     # 结果描述
builtin_interfaces/Duration actual_duration  # 实际执行时长
---
# Feedback
uint8 task_state                   # 当前任务状态
float32 progress_percent           # 进度 0.0~100.0
string current_phase               # 当前阶段描述
string current_subtask             # 当前子任务
builtin_interfaces/Duration elapsed_time  # 已执行时间
```

```
# te_msgs/action/ExecuteCompositeTask.action
# 执行复合任务（多子任务编排）

# Goal
string task_id                     # 任务 ID
string description                 # 任务描述
SubTask[] subtasks                 # 子任务列表
int32 priority                     # 优先级
builtin_interfaces/Duration max_execution_time  # 总超时
---
# Result
bool success                       # 是否成功
ErrorCode error                  # 错误码
string message                     # 结果描述
string[] completed_subtasks        # 已完成的子任务 ID
string[] failed_subtasks           # 失败的子任务 ID
---
# Feedback
uint8 task_state                   # 当前任务状态
float32 overall_progress           # 总体进度 0.0~100.0
string current_subtask_id          # 当前执行的子任务 ID
string current_phase               # 当前阶段
uint32 completed_subtask_count     # 已完成子任务数
uint32 total_subtask_count         # 总子任务数
```

### 4.4 接口汇总表

#### Topics

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/te/task_state` | `te_msgs/msg/TaskState` | TE → ALL | Reliable + Transient Local + Depth 10 | 事件驱动 | 单个任务状态变更广播 |
| `/te/task_list` | `te_msgs/msg/TaskList` | TE → ALL | Reliable + Volatile + Depth 1 | 1 Hz | 任务列表快照 |
| `/te/scheduler_state` | `te_msgs/msg/TeSchedulerState` | TE → ALL | Reliable + Transient Local + Depth 1 | 事件驱动 | 调度器状态广播 |
| `/te/heartbeat` | `te_msgs/msg/Heartbeat` | TE → PS/HDS | Reliable + Volatile + Depth 1 | 1 Hz | TE 心跳 |
| `/sm/robot_state` | `sm_msgs/msg/RobotState` | SM → TE | Reliable + Transient Local + Depth 1 | 事件驱动 | TE 订阅 SM 状态 |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/te/submit_task` | `te_msgs/srv/SubmitTask` | Gateway, Agent | 创建并提交新任务 |
| `/te/cancel_task` | `te_msgs/srv/CancelTask` | Gateway, Agent, SM(E-Stop) | 取消指定任务 |
| `/te/pause_task` | `te_msgs/srv/PauseTask` | Gateway, Agent | 暂停指定任务 |
| `/te/resume_task` | `te_msgs/srv/ResumeTask` | Gateway, Agent | 恢复指定任务 |
| `/te/get_task_status` | `te_msgs/srv/GetTaskStatus` | Gateway, Agent, HDS | 查询任务状态 |
| `/te/get_health_status` | `te_msgs/srv/GetHealthStatus` | EM, HDS | TE 健康状态查询 |
| `/te/control_scheduler` | `te_msgs/srv/ControlScheduler` | Gateway, SM, HDS | 控制调度器状态 |
| `/sm/request_transition` | `sm_msgs/srv/RequestTransition` | TE → SM | 请求 ACTIVE_IDLE ↔ ACTIVE_BUSY 转换 |
| `/sm/is_motion_allowed` | `sm_msgs/srv/IsMotionAllowed` | TE → SM | 运动前状态校验 |

#### Actions

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/te/execute_task` | `te_msgs/action/ExecuteTask` | Gateway, Agent | 执行单任务（长耗时） |
| `/te/execute_composite` | `te_msgs/action/ExecuteCompositeTask` | Gateway, Agent | 执行复合任务 |
| `/mc/execute_motion` | `mc_msgs/action/ExecuteMotion` | TE → MC | 运动模式执行（STAND/WALKING/ZERO_TORQUE 等）|
| `/mp/play_motion` | `mp_msgs/action/PlayMotion` | TE → MP | 动作播放 |
| `/pnc/navigate_to` | `pnc_msgs/action/NavigateTo` | TE → PnC | 导航到目标点 |
| `/agent/execute_interaction` | `agent_msgs/action/ExecuteInteraction` | TE → Agent | 交互执行 |

---

## 5. 内部设计

### 5.1 节点结构

```
┌─────────────────────────────────────────────────────────────────────┐
│                        TaskEngineNode                                │
│                                                                      │
│  ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐   │
│  │  Task Scheduler │   │  Task Executor  │   │  State Manager  │   │
│  │  (任务调度器)   │   │  (任务执行器)   │   │  Client (SM)    │   │
│  │                 │   │                 │   │                 │   │
│  │  - 优先级队列   │   │  - 子任务分发   │   │  - 状态转换请求 │   │
│  │  - 依赖解析     │   │  - Action 调用  │   │  - 运动许可查询 │   │
│  │  - 调度策略     │   │  - 超时监控     │   │  - 状态缓存     │   │
│  └────────┬────────┘   └────────┬────────┘   └─────────────────┘   │
│           │                     │                                     │
│           └──────────┬──────────┘                                     │
│                      ▼                                               │
│  ┌─────────────────────────────────────────┐                         │
│  │         Task Lifecycle Manager          │                         │
│  │    (状态机引擎 + 持久化 + 事件发布)     │                         │
│  └─────────────────────────────────────────┘                         │
│                                                                      │
│  ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐   │
│  │  Action Client  │   │  Service Client │   │  Service Server │   │
│  │  Pool           │   │  Pool           │   │  (TE 对外接口)  │   │
│  │                 │   │                 │   │                 │   │
│  │  - MP Action    │   │  - SM Services  │   │  - SubmitTask   │   │
│  │  - PnC Action   │   │  - Agent Srv    │   │  - CancelTask   │   │
│  │  - Agent Action │   │  - HDS Srv      │   │  - GetStatus    │   │
│  └─────────────────┘   └─────────────────┘   └─────────────────┘   │
│                                                                      │
│  ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐   │
│  │  E-Stop Handler │   │  Heartbeat      │   │  Config Manager │   │
│  │  (独立回调组)   │   │  Timer (1Hz)    │   │  (参数管理)     │   │
│  └─────────────────┘   └─────────────────┘   └─────────────────┘   │
│                                                                      │
│  ┌─────────────────────────────────────────┐                         │
│  │  Composite Task Parser                  │                         │
│  │  (JSON/YAML 复合任务描述解析器)         │                         │
│  └─────────────────────────────────────────┘                         │
└─────────────────────────────────────────────────────────────────────┘
```

### 5.2 关键组件

#### 5.2.1 Task Scheduler（任务调度器）

- **优先级队列**：按 `priority` 降序排列，同优先级按 `created_at` 时间先后
- **并发控制**：默认最大并发任务数 = 1（人形机器人同一时间只能执行一个物理任务），可通过参数调整
- **依赖解析**：复合任务中，子任务按 `depends_on` 字段构建 DAG，拓扑排序后调度
- **调度策略**：
  - `SEQUENTIAL`：顺序执行（默认）
  - `PARALLEL`：并行执行无依赖子任务
  - `PIPELINE`：流水线（前一个子任务的输出作为下一个的输入）

#### 5.2.2 Task Executor（任务执行器）

- **子任务分发**：根据 `subtask_type` 选择对应的 Action Client 调用目标模块
- **异步执行**：所有子任务通过 ROS2 Action 异步调用，不阻塞调度器主线程
- **超时监控**：每个子任务独立计时器，超时时自动取消并标记失败
- **进度聚合**：收集子任务 Feedback，计算并发布总体进度

#### 5.2.3 Task Lifecycle Manager（生命周期管理器）

- 维护每个任务的 FSM 状态
- 状态变更时发布 `/te/task_state` Topic
- 记录任务历史（内存中保留最近 1000 条，供查询）
- 任务结束时自动请求 SM `ACTIVE_BUSY → ACTIVE_IDLE` 转换

#### 5.2.4 E-Stop Handler（急停处理器）

- **独立 CallbackGroup**：订阅 `/sm/robot_state`，使用独立的 `MutuallyExclusive` CallbackGroup
- 收到 `ACTIVE_E_STOP` 或 `FAULT` 状态时：
  1. 立即取消当前所有运行中任务（调用各 Action Client 的 `cancel_goal`）
  2. 发布 `/te/task_state`（状态 = `CANCELLING` → `CANCELLED`）
  3. 请求 SM 状态转换（如需要）
  4. 拒绝所有新任务请求（返回 `ERR_ESTOP_ACTIVE` 或 `ERR_FAULT_STATE`）

### 5.3 关键流程

#### 5.3.1 任务创建与调度流程

```
Gateway/Agent 调用 /te/submit_task
  → Service Server 接收请求
  → 参数校验（task_type, parameters_json 格式）
    - 失败 → 返回 accepted=false, ERR_INVALID_PARAMETERS
  → 生成 task_id（如未提供）
  → 创建 TaskState 对象，状态 = CREATED
  → 检查调度器状态：
    - SCHEDULER_PAUSED → 返回 accepted=false, ERR_SCHEDULER_PAUSED
    - SCHEDULER_FAULT → 返回 accepted=false, ERR_SCHEDULER_FAULT
  → 检查并发限制：
    - 当前运行任务数 >= max_concurrent_tasks → 入队，状态 = SCHEDULED
    - 否则 → 状态 = SCHEDULED，触发调度
  → 返回 accepted=true, task_id
  → 发布 /te/task_state (CREATED)
```

#### 5.3.2 任务执行流程（运动任务示例）

```
Task Scheduler 从队列取出任务
  → 状态 = PREPARING
  → 检查目标模块就绪状态（通过 EM /em/get_process_status）
    - 模块未就绪 → 返回 ERR_MODULE_NOT_READY
  → 调用 /sm/request_transition (ACTIVE_BUSY, priority=40)
    - SM 拒绝 → 状态 = FAILED, 返回 ERR_SM_TRANSITION_REJECTED
  → SM 接受 → 状态 = RUNNING
  → 发布 /te/task_state (RUNNING)
  → Task Executor 解析 parameters_json
  → 调用对应模块 Action：
    - 运动任务 → /mc/execute_motion (Action) 或 /mp/play_motion (Action)
    - 导航任务 → /pnc/navigate_to (Action)
    - 交互任务 → /agent/execute_interaction (Action)
    - 遥操作任务 → /mc/execute_motion (Action, motion_mode=TELEOP)
    - 数据录制任务 → /mc/execute_motion (Action) + DR 录制启动
    - 动作回放任务 → /mp/play_motion (Action)
    - VLA 推理任务 → /mc/execute_motion (Action, motion_mode=INFER)
    - 校准任务 → /mc/execute_motion (Action, motion_mode=CALIBRATE)
    - 模型训练任务 → 后台启动训练进程（不占用实时控制资源）
  → 接收 Action Feedback，更新 progress_percent
  → 发布 /te/task_state (进度更新)
  → Action 完成：
    - success=true → 状态 = COMPLETED
    - success=false → 状态 = FAILED, 记录 error_code
  → 调用 /sm/request_transition (ACTIVE_IDLE, priority=40)
  → 发布 /te/task_state (最终状态)
```

#### 5.3.3 任务取消流程

```
Gateway/Agent/SM(E-Stop) 调用 /te/cancel_task
  → 查找任务
    - 不存在 → 返回 ERR_TASK_NOT_FOUND
    - 状态为 COMPLETED/FAILED/CANCELLED → 返回 ERR_TASK_NOT_CANCELLABLE
  → 状态 = CANCELLING
  → 如果任务正在执行（RUNNING）：
    - 调用对应 Action Client 的 cancel_goal()
    - 等待 Action Result（超时 5s）
  → 清理子任务资源
  → 状态 = CANCELLED
  → 如果当前无其他运行任务 → 请求 SM ACTIVE_BUSY → ACTIVE_IDLE
  → 发布 /te/task_state (CANCELLED)
  → 返回 accepted=true
```

#### 5.3.4 E-Stop 触发时的任务处理流程

```
E-Stop 触发（SM 状态 → ACTIVE_E_STOP）
  → E-Stop Handler（独立回调组）立即收到
  → 遍历所有 RUNNING / PAUSED / PREPARING 状态的任务：
    - 对每个任务调用 cancel_goal()
    - 状态 = CANCELLING
  → 拒绝所有新任务请求（返回 ERR_ESTOP_ACTIVE）
  → 等待取消清理完成
  → 状态 = CANCELLED
  → 发布 /te/task_state (批量更新)
  → 调度器状态 = SCHEDULER_IDLE
  → 上报 HDS：E-Stop 期间的任务中断信息
```

#### 5.3.5 复合任务执行流程

```
Gateway/Agent 调用 /te/execute_composite (Action)
  → 解析 SubTask[] 列表，构建 DAG
  → 拓扑排序，确定执行顺序
  → 按顺序/并行调度子任务：
    - 无依赖的子任务并行启动
    - 有依赖的等待前置完成
  → 每个子任务完成时检查结果：
    - 成功 → 继续后续子任务
    - 失败 → 根据配置决定：
      - fail_fast=true → 整个复合任务失败
      - fail_fast=false → 标记子任务失败，继续其他分支
  → 所有子任务完成 → 聚合结果
  → 返回 Action Result
```

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|------|------|------|------|
| Gateway | Gateway → TE | `/te/submit_task` (Service) | 用户通过 APP/云端下发任务 |
| Gateway | Gateway → TE | `/te/cancel_task` (Service) | 用户取消任务 |
| Gateway | Gateway → TE | `/te/get_task_status` (Service) | 用户查询任务进度 |
| Gateway | TE → Gateway | `/te/task_state` (Topic) | TE 将任务状态同步到云端/APP |
| Agent | Agent → TE | `/te/submit_task` (Service) | Agent 自主决策生成任务 |
| Agent | Agent → TE | `/te/execute_task` (Action) | Agent 执行长耗时交互任务 |
| Agent | TE → Agent | `/agent/execute_interaction` (Action) | TE 调用 Agent 执行交互子任务 |
| SM | TE → SM | `/sm/request_transition` (Service) | 任务开始/结束请求状态转换 |
| SM | TE → SM | `/sm/is_motion_allowed` (Service) | 运动任务前状态校验 |
| SM | SM → TE | `/sm/robot_state` (Topic) | TE 订阅状态，E-Stop 时取消任务 |
| MC | TE → MC | `/mc/execute_motion` (Action) | 调用 MC 执行运动模式（STAND/WALKING/TELEOP/INFER/CALIBRATE 等）|
| MC | TE → MC | `/mc/set_motion_mode` (Service) | 直接切换运动模式（轻量级）|
| MP | TE → MP | `/mp/play_motion` (Action) | 调用动作播放 |
| MS | TE → MS | `/ms/stream_motion` (Action) | 调用运动流执行 |
| PnC | TE → PnC | `/pnc/navigate_to` (Action) | 调用导航 |
| PnC | PnC → TE | Action Feedback | 导航进度反馈 |
| HDS | TE → HDS | `/hds/report_health` (Service) | 上报任务执行异常、超时数据 |
| HDS | HDS → TE | `/te/control_scheduler` (Service) | HDS 触发降级时暂停调度器 |
| EM | TE → EM | `/em/get_process_status` (Service) | 查询目标模块进程状态 |
| DR | TE → DR | `/te/task_state` (Topic) | DR 记录任务执行历史 |
| DR | DR → TE | `/te/get_task_status` (Service) | DR 回放时查询历史任务 |

### 6.2 关键交互时序

#### 正常运动任务执行时序

```
  Gateway      TE          SM          MP          MC
    │           │           │           │           │
    │ submit_task          │           │           │
    ├──────────►│          │           │           │
    │           │          │           │           │
    │           │ request_transition    │           │
    │           │ (ACTIVE_BUSY, pri=40) │           │
    │           │──────────►│           │           │
    │           │ accepted  │           │           │
    │           │◄──────────│           │           │
    │           │          │           │           │
    │           │ is_motion_allowed     │           │
    │           │──────────►│           │           │
    │           │ allowed   │           │           │
    │           │◄──────────│           │           │
    │           │          │           │           │
    │           │ play_motion (Action) │           │
    │           │──────────►│           │           │
    │           │          │           │           │
    │           │◄────Feedback──────────│           │
    │           │ (progress)            │           │
    │           │          │           │           │
    │  task_state          │           │           │
    │◄──────────┤          │           │           │
    │           │          │           │           │
    │           │◄────Result────────────│           │
    │           │ (success)             │           │
    │           │          │           │           │
    │           │ request_transition    │           │
    │           │ (ACTIVE_IDLE, pri=40) │           │
    │           │──────────►│           │           │
    │           │          │           │           │
    │  task_state(COMPLETED)            │           │
    │◄──────────┤          │           │           │
    │           │          │           │           │
```

#### E-Stop 触发时序

```
  Hardware     SM          TE          MP          MC
    │           │           │           │           │
    │ trigger_estop        │           │           │
    ├──────────►│          │           │           │
    │           │          │           │           │
    │           │ robot_state=ACTIVE_E_STOP        │
    │           │──────────►│           │           │
    │           │          │           │           │
    │           │          │ cancel_goal()         │
    │           │          │──────────►│           │
    │           │          │           │ stop      │
    │           │          │           │──────────►│
    │           │          │           │           │
    │           │          │◄──Result──│           │
    │           │          │ (cancelled)           │
    │           │          │           │           │
    │           │          │ task_state=CANCELLED  │
    │           │          │──────────►│           │
    │           │          │ (Gateway) │           │
```

---

## 7. 关键参数与配置

```yaml
# te/config/te_params.yaml

task_engine:
  ros__parameters:
    # 最大并发任务数（人形机器人建议为 1）
    max_concurrent_tasks: 1

    # 任务队列最大深度
    max_queue_depth: 50

    # 默认任务超时（秒）
    default_task_timeout_sec: 300.0

    # 子任务超时（秒）
    default_subtask_timeout_sec: 60.0

    # SM 状态转换请求超时（秒）
    sm_transition_timeout_sec: 5.0

    # SM 运动许可查询超时（秒）
    sm_motion_check_timeout_sec: 1.0

    # 任务准备阶段超时（秒）
    task_prepare_timeout_sec: 10.0

    # 调度超时（等待前置条件）（秒）
    scheduling_timeout_sec: 60.0

    # 心跳发布频率（Hz）
    heartbeat_rate_hz: 1.0

    # 任务状态广播 QoS 深度
    task_state_qos_depth: 10

    # 任务列表发布频率（Hz）
    task_list_publish_rate_hz: 1.0

    # 任务历史保留条数（内存中）
    task_history_size: 1000

    # 复合任务 fail_fast 模式（默认 true）
    composite_fail_fast: true

    # E-Stop 时 Action 取消超时（秒）
    estop_cancel_timeout_sec: 5.0

    # 模块就绪检查重试次数
    module_ready_retry_count: 3

    # 模块就绪检查重试间隔（秒）
    module_ready_retry_interval_sec: 2.0

    # 是否启用详细任务日志
    verbose_task_log: true

    # 任务进度反馈最小间隔（毫秒）
    min_feedback_interval_ms: 100.0
```

---

## 8. 错误码定义

| 错误码 | 常量名 | 说明 | 处理建议 |
|--------|--------|------|---------|
| 0 | `OK` | 成功 | — |
| 4001 | `ERR_INVALID_TASK_TYPE` | 不支持的任务类型 | 检查 task_type 枚举值 |
| 4002 | `ERR_INVALID_PARAMETERS` | 任务参数校验失败 | 检查 parameters_json 格式和内容 |
| 4003 | `ERR_SM_TRANSITION_REJECTED` | SM 状态转换被拒绝 | 检查当前机器人状态，稍后重试 |
| 4004 | `ERR_MOTION_NOT_ALLOWED` | SM 运动许可校验失败 | 当前状态不允许运动，等待状态恢复 |
| 4005 | `ERR_SUBTASK_TIMEOUT` | 子任务执行超时 | 检查目标模块负载，考虑增加超时时间 |
| 4006 | `ERR_SUBTASK_FAILED` | 子任务执行失败 | 查看子模块错误日志 |
| 4007 | `ERR_TASK_CANCELLED` | 任务被取消 | 正常流程，无需处理 |
| 4008 | `ERR_DEPENDENCY_NOT_MET` | 前置依赖未满足 | 检查前置任务是否完成 |
| 4009 | `ERR_SCHEDULER_PAUSED` | 调度器暂停中 | 等待调度器恢复 |
| 4010 | `ERR_SCHEDULER_FAULT` | 调度器故障 | 检查 TE 日志，可能需要重启 TE |
| 4011 | `ERR_MODULE_NOT_READY` | 目标模块未就绪 | 通过 EM 检查模块进程状态 |
| 4012 | `ERR_ESTOP_ACTIVE` | 急停激活，拒绝任务 | 等待人工解除急停 |
| 4013 | `ERR_FAULT_STATE` | FAULT 状态，拒绝任务 | 等待故障确认和恢复 |
| 4014 | `ERR_TASK_NOT_FOUND` | 指定任务不存在 | 检查 task_id 是否正确 |
| 4015 | `ERR_TASK_NOT_CANCELLABLE` | 任务当前状态不可取消 | 任务已完成或已失败 |
| 4016 | `ERR_CONCURRENT_TASK_LIMIT` | 并发任务数达到上限 | 等待当前任务完成或增加并发限制 |
| 4017 | `ERR_INTERNAL_ERROR` | TE 内部错误 | 检查 TE 日志，上报 HDS |

---

## 9. 包结构

```
te_msgs/                      # 消息定义包（纯接口）
├── msg/
│   ├── TaskState.msg           # 任务状态
│   ├── TaskList.msg            # 任务列表
│   ├── SubTask.msg             # 子任务定义
│   ├── TeSchedulerState.msg    # 调度器状态
│   ├── Heartbeat.msg           # TE 心跳（标准命名）
│   └── ErrorCode.msg           # 错误码定义（标准命名）
├── srv/
│   ├── SubmitTask.srv          # 创建任务
│   ├── CancelTask.srv          # 取消任务
│   ├── PauseTask.srv           # 暂停任务
│   ├── ResumeTask.srv          # 恢复任务
│   ├── GetTaskStatus.srv       # 查询任务状态
│   ├── GetHealthStatus.srv     # 健康状态查询
│   └── ControlScheduler.srv    # 控制调度器
├── action/
│   ├── ExecuteTask.action      # 执行任务
│   └── ExecuteCompositeTask.action  # 执行复合任务
├── CMakeLists.txt
└── package.xml

te/                           # 节点实现包
├── include/te/
│   ├── task_engine_node.hpp      # 主节点类
│   ├── task_scheduler.hpp        # 任务调度器
│   ├── task_executor.hpp         # 任务执行器
│   ├── task_lifecycle_manager.hpp # 生命周期管理器
│   ├── composite_task_parser.hpp  # 复合任务解析器
│   ├── estop_handler.hpp         # 急停处理器
│   └── action_client_pool.hpp    # Action Client 池
├── src/
│   ├── task_engine_node.cpp
│   ├── task_scheduler.cpp
│   ├── task_executor.cpp
│   ├── task_lifecycle_manager.cpp
│   ├── composite_task_parser.cpp
│   ├── estop_handler.cpp
│   └── action_client_pool.cpp
├── test/
│   ├── test_task_scheduler.cpp       # 调度器单元测试
│   ├── test_task_executor.cpp        # 执行器单元测试
│   ├── test_lifecycle_manager.cpp    # 生命周期管理测试
│   ├── test_composite_task.cpp       # 复合任务测试
│   ├── test_estop_handler.cpp        # E-Stop 处理测试
│   └── test_integration.cpp          # 集成测试（与 SM/MP 模拟交互）
├── config/
│   └── te_params.yaml              # 参数配置
├── launch/
│   └── task_engine.launch.py       # Launch 文件
├── CMakeLists.txt
└── package.xml
```

---

## 10. 关键性能指标（KPI）

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 任务创建响应延迟 | < 10ms | 从收到 submit_task 到返回 accepted |
| 任务调度延迟 | < 50ms | 从 SCHEDULED 到 PREPARING |
| SM 状态转换请求延迟 | < 20ms | 调用 /sm/request_transition 往返 |
| 子任务分发延迟 | < 10ms | 从 RUNNING 到 Action Goal 发送 |
| E-Stop 任务取消延迟 | < 100ms | 从收到 E-Stop 到 cancel_goal 发出 |
| 任务进度反馈频率 | >= 10Hz | 高频任务（如导航）的反馈发布频率 |
| 心跳抖动 | < 50ms | /te/heartbeat 发布周期抖动 |
| TE 自身 CPU 占用 | < 2% | 空闲时 |
| TE 自身内存占用 | < 50MB | 包含任务历史缓存 |
| 复合任务子任务调度延迟 | < 20ms | 子任务间切换延迟 |
| 任务历史查询延迟 | < 5ms | 查询最近 1000 条任务记录 |

---

## 11. 安全设计

### 11.1 E-Stop 路径

- **独立 CallbackGroup**：E-Stop Handler 使用独立的 `MutuallyExclusive` CallbackGroup，与常规 Service 回调隔离
- **不可被覆盖**：E-Stop 触发时，所有正在执行的任务必须立即取消，不可被新任务抢占
- **SM 状态校验**：每个运动子任务执行前，必须通过 `/sm/is_motion_allowed` 校验；任务执行期间持续监听 `/sm/robot_state`，状态变为 `ACTIVE_E_STOP` 或 `FAULT` 时立即取消
- **取消兜底**：调用 Action Client `cancel_goal()` 后，若 5s 内未收到取消确认，强制标记任务为 CANCELLED 并释放资源

### 11.2 运动安全约束

- **禁止跳过 SM 校验**：任何运动相关子任务（调用 MP/MS/PnC）执行前，必须调用 `/sm/is_motion_allowed`
- **禁止阻塞式 Service 调用**：在 Action Execute 回调中，所有 SM Service 调用使用异步方式（`async_send_request`）
- **状态缓存**：TE 订阅 `/sm/robot_state` 并本地缓存，用于快速拒绝明显非法的任务请求（减少 Service 调用）

### 11.3 故障隔离

- **单任务失败不级联**：一个任务失败不影响其他已调度任务（除非复合任务配置了 fail_fast）
- **子模块故障处理**：子任务 Action 调用失败时，TE 记录错误码，上报 HDS，按配置决定重试或失败
- **TE 自身故障**：TE 崩溃后，由 EM 负责重启；重启后从持久化存储恢复任务队列（如配置了持久化）

### 11.4 审计日志

- 所有任务生命周期事件（创建、状态变更、完成、取消、失败）记录到 `/te/task_state` Topic
- DR 模块订阅该 Topic 持久化存储
- 记录字段：时间戳、任务 ID、状态变更、请求者、错误码

---

## 12. 安全审查记录

**审查时间**：2026-04-29
**审查结果**：APPROVED_WITH_CONDITIONS

### 通过项

1. **E-Stop Handler 独立回调组设计正确** — 5.2.4 节明确 E-Stop Handler 使用独立 `MutuallyExclusive` CallbackGroup，与常规 Service 回调隔离。
2. **SM 状态校验存在且位置正确** — 5.3.2 节任务执行流程中，TE 在 `PREPARING → RUNNING` 阶段调用 `/sm/request_transition` 请求 `ACTIVE_BUSY`，并在子任务执行前调用 `/sm/is_motion_allowed`。
3. **FAULT/ACTIVE_E_STOP 状态拒绝新任务** — 5.3.1 节和 5.2.4 节明确拒绝所有新任务请求（返回 `ERR_ESTOP_ACTIVE` 或 `ERR_FAULT_STATE`）。
4. **Action 取消有兜底超时** — 5.3.3 节明确等待 Action Result（超时 5s）；参数配置中 `estop_cancel_timeout_sec: 5.0`。
5. **异步调用原则已声明** — 11.2 节明确禁止阻塞式 Service 调用。

### 条件通过项（实现阶段必须落实）

1. **E-Stop 时取消 Action 的强制终止机制**：实现阶段必须确认——即使 MP Action 未响应取消，TE 是否仍应请求 SM 释放 `ACTIVE_BUSY` 状态？需在 E-Stop 处理流程中明确独立路径。
2. **SM 状态转换请求超时的故障处理**：区分"SM 明确拒绝"和"SM Service 调用超时/无响应"。建议 SM 调用超时（5s）后立即使任务进入 FAILED，而非等待 prepare_timeout（10s）。
3. **复合任务并行子任务的 E-Stop 取消原子性**：实现阶段必须确保并行子任务的取消是并发发起的（非串行阻塞），且每个子任务的取消独立计时。
4. **任务恢复（Resume）时的 SM 状态重校验缺失**：`PAUSED → RUNNING` 恢复时必须重新调用 `/sm/is_motion_allowed` 或检查 `/sm/robot_state` 缓存，避免绕过安全校验。

### 建议项

1. 增加 `ERR_SM_UNREACHABLE` 错误码，用于 SM Service 调用超时场景。
2. 明确 E-Stop 时 TE 是否请求 SM 状态转换（"如需要"的具体条件）。
3. 任务创建时增加前置 SM 状态校验（基于本地缓存快速拒绝）。
4. 明确 `TE_TYPE_EMERGENCY` 紧急任务类型的安全约束（是否受 SM ACTIVE_E_STOP/FAULT 约束）。
5. 增加 E-Stop 到所有子任务确认取消或强制终止的 KPI（< 200ms）。
