# Executive Manager 模块设计（修订版 v2）

> **修订日期**：2026-04-29
> **修订原因**：架构审查 + 安全审查（第一轮）
> **主要变更**：删除内部状态机（回归无状态执行器）、策略由 SM 持有并下发、L3/L4 上报 HDS 定级、修复 7 项安全漏洞

---

## 1. 模块概述与定位

**模块名称**：Executive Manager（EM）

**定位**：EM 是端侧软件系统中的**无状态执行器**，位于中间件层之下、硬件抽象层之上。它接收来自 SM 的进程编排指令，对端侧所有 ROS2 节点与非 ROS 进程执行启动/停止/监控/恢复操作，本身不做业务状态决策。

EM 填补了三个治理空白：
- systemd 只管 OS 级进程死活，不懂机器人业务语义
- SM 只管状态机决策，不直接操作进程
- HDS 只管诊断定级，不执行恢复动作

EM 将这三者串联：**SM 决策状态 → EM 执行进程操作 → HDS 定级故障**。

**核心职责**：
1. 按 SM 下发的进程集合指令，有序启动/停止进程组
2. 实时监控进程健康状态（心跳、退出码、资源水位）
3. 分级故障恢复（L1/L2 自主执行；L3/L4 上报 HDS 定级）
4. 对外提供统一的进程控制 API，供 SM、TE 调用

**改名后的定位变化**（PS → EM）：

| 维度 | Process Supervisor（原设计） | Executive Manager（修订后） |
|------|---------------------------|----------------------------|
| **核心隐喻** | "看护进程"（进程挂了重启） | "无状态执行器"（SM 决策，EM 执行） |
| **SM 联动** | 被动接收启动/停止指令 | **主动订阅 SM 状态**，按指令执行进程编排 |
| **状态机** | 无 | **无**（删除 v1 中的 9 状态内部状态机） |
| **策略映射** | 固定进程组配置 | **由 SM 持有策略**，EM 接收进程集合指令 |
| **故障响应** | L3/L4 自主请求 SM 状态转换 | **L3/L4 上报 HDS**，由 HDS 定级后请求 SM |

**关键结论**：EM 是**纯执行器**，不做状态决策、不做策略映射、不做故障定级。所有业务语义由 SM（状态）和 HDS（故障）持有。

---

## 2. 职责边界

EM 在端侧架构中的位置：

```mermaid
flowchart TB
    Gateway["Gateway
云端/APP 通信（唯一云端出口）"]
    SM["SM
状态机决策（FSM 唯一权威）"]
    TE["TE
任务调度"]
    EM["EM（本模块）
无状态执行器（启动/监控/恢复）"]
    HDS["HDS
故障诊断与定级（唯一定级权威）"]
    Systemd["systemd
OS 级进程管理"]
    HAL["HAL
硬件抽象"]

    Gateway --> SM --> TE --> EM --> HDS --> Systemd --> HAL
```

| 边界 | EM 负责 | 对方负责 | 红线 |
|------|---------|---------|------|
| EM ↔ SM | 订阅 SM 状态；接收进程编排指令并执行；上报执行进度 | 状态机决策；定义状态→进程集合映射 | EM **不**决定什么状态下该运行什么进程 |
| EM ↔ HDS | 上报进程状态变更、恢复动作记录、L3/L4 建议 | 故障诊断与定级；决定是否请求 SM 状态转换 | EM **不**做故障定级；**不**直接请求 SM 状态转换 |
| EM ↔ systemd | 通过 dbus/API 启动/停止/查询进程 | 内核态守护、开机自启、cgroup | — |
| EM ↔ TE | 接收 TE 的任务级进程调度请求 | 任务编排 | TE **不**直接操作进程 |
| EM ↔ 各模块 | 监控进程心跳，执行重启/停止 | 业务逻辑，上报自身心跳 | 各模块 **不**自行启停其他进程 |

**三大红线**（不可违反）：
1. EM **不做**业务状态决策（SM 是唯一状态权威）
2. EM **不做**故障定级（HDS 是唯一定级权威）
3. EM **不**直接操作硬件（HAL 负责）

---

## 3. 执行上下文感知

### 3.1 设计原则

EM 本身**没有内部状态机**，但具备**执行上下文感知**能力：
- 订阅 `/sm/robot_state` Topic，了解当前机器人 FSM 状态
- 根据 SM 下发的进程集合指令，执行启动/停止编排
- 维护**执行阶段**（ExecutionPhase）—— 这是操作进度，不是业务状态

### 3.2 ExecutionPhase（执行阶段，非状态机）

| 阶段 | 说明 |
|------|------|
| `IDLE` | 无正在执行的编排动作 |
| `STARTING` | 正在执行启动编排 |
| `STOPPING` | 正在执行停止编排 |
| `RECOVERING` | 正在执行 L1/L2 恢复 |

> **注意**：ExecutionPhase 只表达"EM 当前在做什么操作"，不表达"机器人处于什么业务状态"。业务状态由 SM FSM 唯一持有。

### 3.3 SM 状态 → 进程集合映射

**该映射由 SM 持有并维护**，EM 不硬编码。SM 在状态转换时，通过 Service 调用 EM 的 `ApplyProcessSet`，传入目标进程组列表：

```
SM FSM 状态变更（如 STANDBY → ACTIVE_IDLE）
    ↓
SM 查询内部映射表：ACTIVE_IDLE → ["perception", "control", "ai"]
    ↓
SM 调用 EM::ApplyProcessSet(groups=["perception", "control", "ai"])
    ↓
EM 计算差异：需要启动 perception + control + ai
    ↓
EM Orchestrator 按 DAG 顺序执行
    ↓
EM 上报进度（STARTING → IDLE）
    ↓
SM 通过 GetProcessStatus 查询确认
```

---

## 4. ROS2 接口定义

### 4.1 消息定义（msg）

```
# em_msgs/msg/ProcessStatus.msg
# 单个进程状态

uint8 UNKNOWN     = 0
uint8 STARTING    = 1
uint8 RUNNING     = 2
uint8 DEGRADED    = 3
uint8 STOPPED     = 4
uint8 FAILED      = 5
uint8 RESTARTING  = 6

string process_name        # 进程名（唯一标识）
string process_group       # 所属进程组
uint8 state                # 进程状态
int32 pid                  # OS 进程 ID（整数类型）
builtin_interfaces/Time last_heartbeat  # 最后心跳时间
float32 cpu_percent        # CPU 使用率
float32 memory_mb          # 内存使用（MB）
uint32 restart_count       # 累计重启次数
int32 exit_code            # 上次退出码（整数类型）
```

```
# em_msgs/msg/ProcessGroupConfig.msg
# 进程组配置

string group_name          # 组名（infra/middleware/ops/perception/control/ai）
uint8 priority             # 优先级（P0=0, P1=1, P2=2, P3=3）
string[] process_names     # 组内进程列表
string[] depends_on        # 依赖的进程组（DAG 边）
bool auto_restart          # 是否自动重启
uint8 max_restarts         # 最大重启次数（默认 3）
uint32 restart_window_sec  # 重启窗口（默认 300s）
string readiness_probe     # 就绪探针类型（tcp/port/topic/custom）
```

```
# em_msgs/msg/ExecutionProgress.msg
# 执行进度（替代原 EmState）

uint8 IDLE       = 0
uint8 STARTING   = 1
uint8 STOPPING   = 2
uint8 RECOVERING = 3

uint8 phase                # 当前执行阶段
string current_action      # 当前动作描述（如"启动 motion_control"）
uint8 progress_percent     # 进度百分比
string[] running_processes         # 当前运行中的进程列表
string[] failed_processes          # 当前失败进程列表
uint32 restart_count_this_session  # 本次开机累计重启次数
```

### 4.2 服务定义（srv）

统一为 SM 风格的动宾命名：

```
# em_msgs/srv/ControlProcess.srv
# 控制单个进程或进程组

--- 请求 ---
uint8 START   = 0
uint8 STOP    = 1
uint8 RESTART = 2

string target              # 进程名或进程组名
uint8 action               # START / STOP / RESTART
bool force                 # 是否强制操作（仅 SM/HDS Master 可设为 true）
string requester_node      # 请求者节点名（审计用）
--- 响应 ---
bool success
uint16 error_code          # 扁平错误码（与 SM 风格一致）
string message             # 可读错误描述
ProcessStatus[] affected_processes
```

```
# em_msgs/srv/GetHealthStatus.srv
# EM 模块健康状态查询

---
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
ProcessStatus[] processes
ExecutionProgress progress  # EM 执行进度
```

```
# em_msgs/srv/GetProcessStatus.srv
# 查询进程或 EM 执行进度

--- 请求 ---
string target              # 进程名、进程组名，或空字符串表示查询 EM 自身进度
--- 响应 ---
bool success
uint16 error_code
string message
ProcessStatus[] processes
ExecutionProgress progress  # EM 执行进度（target 为空时有效）
```

```
# em_msgs/srv/ApplyProcessSet.srv
# 应用进程集合（由 SM 调用，替代原 ApplyStrategy）

--- 请求 ---
string[] groups_to_start   # 需要启动的进程组列表
string[] groups_to_stop    # 需要停止的进程组列表
string reason              # 切换原因
string requester_node      # 请求者节点名
--- 响应 ---
bool success
uint16 error_code
string message
string[] started_groups    # 实际启动的组
string[] stopped_groups    # 实际停止的组
```

```
# em_msgs/srv/ReloadConfiguration.srv
# 运行时热重载配置

--- 请求 ---
string config_path         # 配置文件路径（必须在白名单 /opt/striding/em/ 下）
string requester_node      # 请求者节点名（审计用）
--- 响应 ---
bool success
uint16 error_code
string message
string[] changed_processes
```

```
# em_msgs/srv/AcknowledgeEstopRelease.srv
# E-Stop 解除确认（EM 向 SM 查询人工确认状态）

--- 请求 ---
string operator_id         # 操作者 ID（由 SM 提供）
--- 响应 ---
bool confirmed             # SM 是否确认该 operator_id 有效
uint16 error_code
string message
```

### 4.3 Action 定义（action）

```
# em_msgs/action/SystemRestart.action
# 全系统有序重启

--- 目标 ---
string reason              # 重启原因
bool preserve_state        # 是否保留当前 SM 状态
--- 反馈 ---
uint8 phase                # 当前阶段（0=停止中, 1=等待中, 2=启动中）
uint8 progress_percent     # 进度百分比
string current_action      # 当前执行的动作描述
--- 结果 ---
bool success
uint16 error_code
string message
builtin_interfaces/Duration duration
```

### 4.4 Topic 定义

#### ROS2 Topic

| Topic | 类型 | 方向 | 说明 |
|-------|------|------|------|
| `/em/execution_progress` | `ExecutionProgress` | EM → 所有 | EM 执行进度广播（1Hz） |
| `/em/process_status` | `ProcessStatus[]` | EM → 所有 | 所有进程状态快照（1Hz） |
| `/em/events` | `TransitionEvent` | EM → 所有 | 状态变更/崩溃/恢复事件 |
| `/sm/robot_state` | `RobotState` | SM → EM | 订阅 SM 全局状态 |
| `/hds/diagnosis_result` | `DiagnosisResult` | HDS → EM | 订阅 HDS 诊断结果 |
| `/em/process_heartbeat` | `Heartbeat` | 各模块 → EM | 各模块向 EM 上报心跳 |
| `/em/estop_immediate` | `Empty` | EM → 所有 | E-Stop 紧急广播（0 延迟） |
| `/em/control_events` | `ControlProcess` | EM → 所有 | 控制操作审计广播 |

#### MQTT 事件（EM ↔ HDS 跨语言通信）

| Topic | 方向 | 说明 |
|-------|------|------|
| `em/event/process_state` | EM → HDS | 进程状态变更事件 |
| `em/event/recovery` | EM → HDS | 恢复动作执行记录 |
| `em/event/l3_suggestion` | EM → HDS | L3 降级建议（HDS 定级后决定是否请求 SM） |
| `em/event/l4_suggestion` | EM → HDS | L4 紧急停止建议（HDS 定级后决定是否请求 SM） |
| `hds/command/recovery_ack` | HDS → EM | HDS 批准/拒绝恢复建议 |

---

## 5. 内部设计

### 5.1 节点结构

```mermaid
flowchart TB
    subgraph EMNode["executive_manager_node"]
        subgraph MainNode["rclcpp::Node (主节点，普通用户权限)"]
            Orchestrator["Orchestrator (编排引擎)"]
            HealthMonitor["HealthMonitor (健康监控)"]
            RecoveryEngine["RecoveryEngine (恢复引擎)"]
            ProcessRegistry["ProcessRegistry (进程注册表)"]
            ConfigManager["ConfigManager (配置管理)"]
            TransitionQueue["TransitionQueue (编排队列)"]
        end

        subgraph EStop["EStopHandler (独立节点/线程)"]
            EStopExec["独立 Executor（SingleThreadedExecutor）"]
            EStopSub["/sm/robot_state 订阅"]
            EStopPub["/em/estop_immediate Publisher"]
            EStopUDS["Unix Domain Socket → em_daemon"]
        end

        subgraph Daemon["em_daemon (root 特权代理，最小化代码)"]
            UDS["UDS Server"]
            DBus["systemd dbus 调用"]
            Kill["kill/send_signal"]
        end
    end
```

### 5.2 关键组件

#### 5.2.1 Orchestrator（编排引擎）

- 基于 DAG 计算进程启动/停止顺序
- 支持并行启动无依赖进程
- 就绪探针（Readiness Probe）：
  - `tcp`: 检测 TCP 端口是否监听
  - `port`: 检测 UDP 端口
  - `topic`: 检测 ROS2 Topic 是否有首帧发布
  - `custom`: 调用自定义健康检查接口
- 启动超时（默认 30s）后标记 FAILED，触发 RecoveryEngine
- **编排前检查**：每次编排前检查 `estop_active` 原子标志位，若为 true 则取消编排

#### 5.2.2 HealthMonitor（健康监控）

- 心跳收集：各模块通过 `/em/process_heartbeat` Topic 上报
- **差异化心跳策略**：
  - 常规进程：1Hz，连续 3 次超时视为故障
  - **control 组进程**：10Hz，1 次超时即告警，2 次即触发恢复
  - MC 进程崩溃：通过 `waitpid` 立即检测（< 100ms）
- 监控维度：进程存活、心跳超时、资源水位（CPU > 90% 持续 10s）、退出码异常、ROS2 节点活跃

#### 5.2.3 RecoveryEngine（恢复引擎）

| 级别 | 策略 | 触发条件 | 限制 | EM 行为 |
|------|------|---------|------|---------|
| **L1** | 快速重启 | 单个进程心跳超时/崩溃 | 5min 内最多 3 次 | EM 自主执行，上报 HDS Slave |
| **L2** | 进程组重启 | L1 达到上限 | 10min 内最多 2 次 | EM 自主执行，上报 HDS Slave + SM |
| **L3** | 系统降级建议 | L2 达到上限或多组故障 | — | EM **停止 P2/P3 进程**，通过 MQTT 上报 HDS Master "建议降级"，**等待 HDS 决定是否请求 SM 进入 DEGRADED** |
| **L4** | 紧急停止建议 | P0-Critical 进程失败且不可恢复 | — | EM **立即停止运动进程**，通过 MQTT 上报 HDS Master "建议 E-Stop"，**等待 HDS 决定是否请求 SM 触发 ACTIVE_E_STOP** |

**P0-Critical 定义**（L4 触发范围）：
- `hal_ethercat`（运动控制硬件接口）
- `HAL_Sensor` 中涉及安全监控的部分（如 IMU 异常检测）
- **不包括**：`HAL_AUDIO`（仅触发 DEGRADED）

#### 5.2.4 ProcessRegistry（进程注册表）

- 内存中的进程元数据存储
- 记录：配置信息、当前状态、历史重启记录、依赖关系
- Orchestrator 和 HealthMonitor 的共享数据中心

#### 5.2.5 ConfigManager（配置管理）

- 读取 YAML 配置文件（`/opt/striding/em/config.yaml`）
- **配置校验**：加载时检查 DAG 无环、所有进程路径存在、依赖满足
- **配置签名**：配置文件必须附带 SHA256 签名，ConfigManager 校验签名有效后才加载
- **热重载限制**：
  - 禁止在 E-Stop 或 FAULT 状态下热重载
  - 配置路径必须在白名单 `/opt/striding/em/` 下
  - 仅允许增加/修改，不允许删除运行中的进程
  - 热重载操作记录安全审计日志

#### 5.2.6 TransitionQueue（编排队列）

- 同一时刻只允许一个编排动作执行
- 新请求按优先级排队或抢占
- **E-Stop 必须能抢占任何正在执行的编排**（最高优先级）
- 排队中的请求可通过 `/em/execution_progress` 查询

#### 5.2.7 EStopHandler（急停处理器）

- **独立 SingleThreadedExecutor**，与主节点完全隔离
- 订阅 `/sm/robot_state`，监听 `ACTIVE_E_STOP`
- 检测到 E-Stop 后：
  1. 立即设置 `estop_active = true`（原子标志位）
  2. 向 Orchestrator 发送"取消所有编排"指令
  3. 广播 `/em/estop_immediate` Topic（0 延迟）
  4. 等待 MC 进入硬件级安全模式（500ms 超时）
  5. 通过 UDS 通知 `em_daemon` 停止运动相关进程
- **E-Stop 解除**：EM 通过 `AcknowledgeEstopRelease` Service 向 SM 查询 `operator_id` 有效性，确认后才恢复

---

## 6. 与其他模块的交互

### 6.1 系统冷启动流程

```
systemd 开机
    ↓
systemd 拉起 em_daemon（root）和 executive_manager_node（普通用户）
    ↓
EM 加载 /opt/striding/em/config.yaml（校验签名 + DAG 无环）
    ↓
EM 进入 IDLE，等待 SM 指令
    ↓
SM 启动完成（BOOTING 阶段），调用 EM::ApplyProcessSet
        groups_to_start: ["infra", "middleware", "ops"]
    ↓
EM Orchestrator 按 DAG 启动 infra → middleware → ops
    ↓
所有就绪探针通过
    ↓
SM 广播 FSM: BOOTING → STANDBY
    ↓
SM 调用 EM::ApplyProcessSet（如需要 perception + control）
        groups_to_start: ["perception", "control", "ai"]
    ↓
EM 编排启动 perception → control → ai
    ↓
全部就绪，冷启动目标 < 60s
```

### 6.2 SM 状态变更 → EM 执行流程

```
SM FSM 状态变更（如 STANDBY → ACTIVE_IDLE）
    ↓
SM 查询内部映射表：ACTIVE_IDLE → ["perception", "control", "ai"]
    ↓
SM 调用 EM::ApplyProcessSet
        groups_to_start: ["perception", "control", "ai"]
        reason: "Enter ACTIVE_IDLE"
        requester_node: "state_manager"
    ↓
EM 计算差异：需要启动 perception + control + ai
    ↓
EM Orchestrator 按 DAG 顺序执行
        1. 启动 perception 组（并行启动 VSLAM / Lidar-SLAM / MapManager）
        2. perception 就绪后启动 control 组
        3. control 就绪后启动 ai 组
    ↓
所有就绪探针通过
    ↓
EM 上报 ExecutionProgress: IDLE
    ↓
SM 通过 GetProcessStatus 查询确认
```

### 6.3 故障恢复流程（L1→L2→L3/L4 上报）

```
HealthMonitor 检测到 PnC 心跳超时
    ↓
通知 RecoveryEngine
    ↓
L1: RecoveryEngine 重启 PnC（原地重启）
    ↓
PnC 重启成功 → 恢复正常
    ↓
（若 L1 失败，5min 内 3 次）
L2: RecoveryEngine 重启整个 control 组
    ↓
control 组重启成功 → 恢复正常
    ↓
（若 L2 失败）
L3: RecoveryEngine 执行以下动作：
        1. 停止 perception + control + ai（P2/P3 进程）
        2. 保留 infra + middleware + ops 运行
        3. 通过 MQTT 上报 HDS Master：
          "em/event/l3_suggestion"
          { "reason": "control group restart exhausted",
            "stopped_groups": ["perception", "control", "ai"] }
    ↓
HDS Master 收到建议，进行故障定级
    ↓
HDS 决定请求 SM 进入 DEGRADED（或保持当前状态）
    ↓
SM 批准 DEGRADED，调用 EM::ApplyProcessSet 确认进程集合
    ↓
EM 执行（如已停止则无需操作）
```

### 6.4 E-Stop 触发流程（安全关键路径）

```
E-Stop 触发（硬件按钮 / HDS / SM）
    ↓
SM 立即进入 ACTIVE_E_STOP（priority=100，不可被覆盖）
    ↓
EStopHandler（独立 Executor）订阅到状态变更
        ⚠️ 独立线程，延迟目标 < 10ms
    ↓
EStopHandler 执行：
        1. estop_active = true（原子标志位）
        2. 向 Orchestrator 发送"取消所有编排"
        3. 广播 /em/estop_immediate（0 延迟）
        4. MC 收到 estop_immediate，进入硬件级安全模式
        5. 等待 500ms（MC 安全模式确认超时）
        6. em_daemon 停止运动相关进程：
          - control 组：SIGTERM → SIGKILL（500ms 超时，硬编码）
          - perception 组：SIGTERM → SIGKILL（5s 超时）
          - ai 组：SIGTERM → SIGKILL（5s 超时）
    ↓
保留运行：infra + middleware + ops
    ↓
EM 通过 MQTT 上报 HDS Master：E-Stop 已执行
    ↓
等待人工解除 E-Stop（Gateway → SM）
    ↓
EM 通过 AcknowledgeEstopRelease Service 向 SM 查询 operator_id 有效性
    ↓
确认有效后，estop_active = false
    ↓
SM 调用 EM::ApplyProcessSet 恢复 perception + control
```

---

## 7. 关键参数与配置

### 7.1 运行时参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `heartbeat_period_sec` | 1.0 | 常规进程心跳上报周期 |
| `heartbeat_period_control_sec` | 0.1 | control 组进程心跳周期（10Hz） |
| `heartbeat_timeout_count` | 3 | 常规进程连续超时次数 |
| `heartbeat_timeout_count_control` | 2 | control 组连续超时次数 |
| `startup_timeout_sec` | 30.0 | 进程启动超时时间 |
| `l1_max_restarts` | 3 | L1 快速重启最大次数 |
| `l1_restart_window_sec` | 300 | L1 重启窗口（秒） |
| `l2_max_restarts` | 2 | L2 进程组重启最大次数 |
| `l2_restart_window_sec` | 600 | L2 重启窗口（秒） |
| `cpu_threshold_percent` | 90.0 | CPU 使用率告警阈值 |
| `cpu_threshold_duration_sec` | 10.0 | CPU 持续超阈值时间 |
| `config_path` | `/opt/striding/em/config.yaml` | 配置文件路径 |
| `config_signature_required` | true | 是否强制要求配置签名 |
| `estop_immediate_timeout_ms` | 500 | E-Stop 等待 MC 安全模式超时（ms，硬编码最小值） |

### 7.2 配置文件结构

```yaml
# /opt/striding/em/config.yaml

signature: "sha256:abc123..."  # 配置文件 SHA256 签名
version: "1.0.0"

processes:
  - name: "broker"
    group: "middleware"
    command: "/opt/striding/bin/broker"
    args: ["--config", "/opt/striding/config/broker.yaml"]
    env: { "ROS_DOMAIN_ID": "0" }
    readiness_probe:
      type: "tcp"
      port: 1883
      timeout: 5
    restart_policy:
      auto_restart: true
      max_restarts: 3
      window_sec: 300
    safety_class: "base"  # base / critical（L4 触发依据）

  - name: "motion_control"
    group: "control"
    command: "/opt/striding/bin/motion_control"
    depends_on: ["state_manager", "hal_ethercat"]
    readiness_probe:
      type: "topic"
      topic: "/mc/joint_states"
      timeout: 15
    restart_policy:
      auto_restart: true
      max_restarts: 3
      window_sec: 300
    safety_class: "critical"  # P0-Critical，故障时可能触发 L4

  - name: "hal_ethercat"
    group: "infra"
    command: "/opt/striding/bin/hal_ethercat"
    readiness_probe:
      type: "tcp"
      port: 502
      timeout: 5
    safety_class: "critical"  # P0-Critical

# ... 其他进程定义

groups:
  - name: "infra"
    priority: 0
    processes: ["hal_sensor", "hal_camera", "hal_lidar", "hal_audio", "hal_ethercat"]

  - name: "middleware"
    priority: 1
    processes: ["broker", "gateway", "state_manager", "executive_manager", "setting"]
    depends_on: ["infra"]

  - name: "ops"
    priority: 1
    processes: ["task_engine", "health_diagnosis", "resource_collection", "data_recorder", "ota"]
    depends_on: ["middleware"]

  - name: "perception"
    priority: 2
    processes: ["perception", "vslam", "lidar_slam", "map_manager"]
    depends_on: ["middleware"]

  - name: "control"
    priority: 2
    processes: ["pnc", "motion_control", "motion_player", "motion_streamer"]
    depends_on: ["perception"]

  - name: "ai"
    priority: 3
    processes: ["agent", "interaction"]
    depends_on: ["middleware"]
```

---

## 8. 错误码定义

统一为扁平风格（与 SM 一致）：

| 错误码 | 名称 | 说明 |
|--------|------|------|
| 0 | OK | 成功 |
| 2001 | ERR_CONFIG_LOAD_FAILED | 配置文件加载失败 |
| 2002 | ERR_CONFIG_INVALID | 配置格式错误或 DAG 有环 |
| 2003 | ERR_CONFIG_SIGNATURE_INVALID | 配置签名校验失败 |
| 2004 | ERR_PROCESS_START_FAILED | 进程启动失败 |
| 2005 | ERR_PROCESS_NOT_FOUND | 指定进程不存在 |
| 2006 | ERR_PROCESS_ALREADY_RUNNING | 进程已在运行 |
| 2007 | ERR_PROCESS_STOP_FAILED | 进程停止失败 |
| 2008 | ERR_HEARTBEAT_TIMEOUT | 进程心跳超时 |
| 2009 | ERR_RESTART_EXHAUSTED | 重启次数耗尽 |
| 2010 | ERR_RECOVERY_FAILED | 故障恢复失败 |
| 2011 | ERR_INVALID_PROCESS_SET | 无效的进程集合 |
| 2012 | ERR_ORCHESTRATION_TIMEOUT | 编排超时 |
| 2013 | ERR_ORCHESTRATION_CANCELLED | 编排被 E-Stop 取消 |
| 2014 | ERR_ESTOP_ACTIVE | 急停状态下拒绝启动运动进程 |
| 2015 | ERR_ESTOP_RELEASE_UNCONFIRMED | E-Stop 解除未经过人工确认 |
| 2016 | ERR_ESTOP_RELEASE_INVALID_OPERATOR | E-Stop 解除的操作者 ID 无效 |
| 2017 | ERR_RELOAD_NOT_ALLOWED | 当前状态下不允许热重载 |
| 2018 | ERR_RELOAD_PATH_NOT_IN_WHITELIST | 配置路径不在白名单中 |
| 2019 | ERR_FORCE_NOT_AUTHORIZED | 请求者无权使用 force 操作 |
| 2020 | ERR_FAULT_HALT | FAULT 状态下拒绝启动非恢复进程 |
| 2021 | ERR_RECOVERY_PRECHECK_FAILED | FAULT 恢复前本地检查失败 |

---

## 9. 包结构

```
em_msgs/                      # 消息定义包（纯接口）
    msg/
        ProcessStatus.msg
        ProcessGroupConfig.msg
        ExecutionProgress.msg
        ErrorCode.msg          # 错误码定义（原名 EmErrorCode.msg）
    srv/
        ControlProcess.srv
        GetProcessStatus.srv
        ApplyProcessSet.srv
        ReloadConfiguration.srv
        AcknowledgeEstopRelease.srv
    action/
        SystemRestart.action
    CMakeLists.txt

executive_manager/            # 节点实现包
    include/executive_manager/
        orchestrator.hpp
        health_monitor.hpp
        recovery_engine.hpp
        process_registry.hpp
        config_manager.hpp
        transition_queue.hpp
        estop_handler.hpp
    src/
        main.cpp
        orchestrator.cpp
        health_monitor.cpp
        recovery_engine.cpp
        process_registry.cpp
        config_manager.cpp
        transition_queue.cpp
        estop_handler.cpp
    em_daemon/                # root 特权代理（最小化）
        src/
            em_daemon.cpp
        CMakeLists.txt
    config/
        em_params.yaml
        config.yaml.template
    launch/
        executive_manager.launch.py
    CMakeLists.txt
```

---

## 10. 安全设计

### 10.1 E-Stop 路径（最高优先级）

- **独立 Executor**：EStopHandler 使用独立的 `SingleThreadedExecutor`，与主节点完全隔离
- **延迟目标**：SM 发布 ACTIVE_E_STOP 到 EM 开始执行 < 10ms
- **原子标志位**：`estop_active` 原子变量，Orchestrator 每次操作前检查
- **零延迟广播**：`/em/estop_immediate` Topic 广播，MC 先进入硬件级安全模式再停止进程
- **SIGKILL 硬编码**：运动进程 SIGTERM 超时 500ms 后强制 SIGKILL，不可通过参数调大
- **人工确认校验**：E-Stop 解除前，EM 通过 `AcknowledgeEstopRelease` Service 向 SM 校验 `operator_id` 有效性
- **审计日志**：所有 E-Stop 触发/解除/操作记录到独立的安全审计日志（`/var/log/striding/em_security.log`），包含时间戳、触发者、原因、operator_id

### 10.2 FAULT 状态下的行为

- FAULT 状态下，EM **拒绝**所有进程启动请求（除人工确认的恢复流程）
- EM **保持** infra + middleware 运行（维持基本通信和诊断能力）
- 从 FAULT 恢复必须经过：
  1. 人工确认（Gateway → SM）
  2. SM 调用 EM::GetProcessStatus 查询
  3. EM 执行本地检查：所有 P0/P1 进程状态为 RUNNING、配置无变更、无待处理恢复任务
  4. EM 本地检查通过后，方可执行恢复编排

### 10.3 权限最小化

- **拆分架构**：
  - `executive_manager_node`：以普通用户运行，ROS2 节点，接收所有 ROS2 请求
  - `em_daemon`：以 root 运行，最小化代码（< 500 行），只暴露 Unix Domain Socket 接口，负责 systemd 调用和信号发送
- **Capabilities 替代方案**（如拆分不可行）：使用 `CAP_KILL` + `CAP_SYS_ADMIN` 子集替代完整 root
- **配置文件权限**：600（仅 root 可读写），防止敏感信息泄露
- **审计日志权限**：640（仅 root 和审计组可读）

### 10.4 Force 操作约束

- `force=true` 仅允许以下节点使用：SM、HDS Master
- EM 维护 `force` 操作白名单，非白名单节点设置 `force=true` 时返回 `ERR_FORCE_NOT_AUTHORIZED`
- `force=true` **不能绕过**安全状态约束：E-Stop 激活和 FAULT 状态下，即使 `force=true` 也拒绝启动运动相关进程
- 所有 `force=true` 的操作记录到安全审计日志

### 10.5 热重载安全

- 禁止在 E-Stop 激活和 FAULT 状态下执行 ReloadConfiguration
- 配置路径必须在白名单 `/opt/striding/em/` 下
- 配置文件必须附带 SHA256 签名，ConfigManager 校验通过后才加载
- 热重载前执行完整校验：DAG 无环、所有进程路径存在、依赖满足
- 热重载操作记录安全审计日志（requester_node、reason、变更内容）

---

## 11. 设计审查修订记录

### v1 → v2 主要变更

| 审查问题 | 严重程度 | v1 设计 | v2 修订 |
|---------|---------|---------|---------|
| EM 内部状态机构成双重状态权威 | P0 | 9 状态内部状态机 | **删除状态机**，改为 ExecutionPhase（操作进度，非业务状态） |
| 策略模板隐式做状态映射 | P0 | EM 内部硬编码策略映射 | **策略由 SM 持有**，EM 接收 `ApplyProcessSet` 指令 |
| EM 自主请求 SM 状态转换 | P1 | L3/L4 直接请求 SM | **L3/L4 上报 HDS**，由 HDS 定级后请求 SM |
| MQTT 通道被移除 | P1 | 纯 ROS2 接口 | **保留 MQTT** 作为 EM ↔ HDS 跨语言通道 |
| Service 命名风格不一致 | P2 | `/em/control`、`/em/get_status` | 统一为动宾命名：`/em/control_process`、`/em/get_process_status` |
| 错误码风格不一致 | P2 | `EmErrorCode` 嵌套消息 | **扁平化**：每个 srv 响应中直接使用 `uint16 error_code` + `string message` |
| E-Stop 解除未校验人工确认 | 安全 | E-Stop 解除后直接恢复 | **新增 `AcknowledgeEstopRelease` Service**，校验 `operator_id` |
| L4 触发条件过于粗糙 | 安全 | "P0 进程失败即触发 L4" | **细分 P0-Critical**（hal_ethercat 等）和 P0-Base（HAL_AUDIO 等） |
| `force` 字段缺乏权限约束 | 安全 | 任何节点可设置 `force=true` | **白名单限制**（仅 SM/HDS Master），不能绕过安全状态 |
| ReloadConfig 缺乏安全校验 | 安全 | 任意路径可热重载 | **路径白名单 + 签名校验 + 状态限制** |
| root 权限未最小化 | 安全 | EM 以 root 运行 | **拆分架构**：`executive_manager_node`（普通用户）+ `em_daemon`（root，最小化） |
| E-Stop 时序未考虑硬件安全 | 安全 | 直接 SIGTERM 停止 MC | **先广播 `estop_immediate`**，MC 进入硬件安全模式后再停止 |
| FAULT 恢复缺少本地检查 | 安全 | SM 检查后直接恢复 | **EM 本地检查**：P0/P1 全部 RUNNING、配置无变更 |
| EStopCallbackGroup 实现不明确 | 条件 | "独立 CallbackGroup" | **明确为独立 SingleThreadedExecutor**，延迟目标 < 10ms |
| 缺少 TransitionQueue | 条件 | 无并发控制 | **新增 TransitionQueue**，E-Stop 可抢占任何编排 |
| 心跳对运动进程不够敏感 | 条件 | 统一 1Hz + 3 次超时 | **control 组 10Hz + 2 次超时**，MC 崩溃 `waitpid` 立即检测 |
| `/em/control` Topic 语义混乱 | 建议 | 混合控制输入和事件广播 | **删除 Topic**，改为 `/em/control_events` 专用于审计广播 |
| `pid` 字段类型 | 建议 | `string pid` | **`int32 pid`** |
| 策略模板过度工程化 | 建议 | 独立 `ExecutionStrategy` 消息类型 | **删除**，进程集合由 SM 直接下发 |
| 心跳 Topic 命名歧义 | 建议 | `/em/heartbeat` | **`/em/process_heartbeat`**（输入）+ `/em/heartbeat`（EM 自身心跳） |

---

## 附录：PS → EM 改名对照表

| 原 PS 术语 | 新 EM 术语（v2） | 说明 |
|-----------|----------------|------|
| Process Supervisor | Executive Manager | 模块名 |
| PS 状态机（无） | 无状态机 | 删除 v1 中的 9 状态内部状态机 |
| 启动编排 | Orchestrator | 功能保留 |
| 健康监控 | HealthMonitor | 新增差异化心跳策略 |
| 恢复引擎 | RecoveryEngine | L3/L4 改为上报 HDS，不直接请求 SM |
| 进程注册表 | ProcessRegistry | 功能保留 |
| 配置管理 | ConfigManager | 新增签名校验 + 热重载安全限制 |
| StateMachine（新增） | 删除 | 回归无状态执行器 |
| ExecutionStrategy（新增） | 删除 | 策略由 SM 持有并下发 |
| Control API（gRPC） | ROS2 Service | 统一接口风格 |
| EStopCallbackGroup | EStopHandler + 独立 Executor | 更明确的实现 |
| 无 | TransitionQueue | 新增编排队列 |
| 无 | em_daemon | 新增 root 特权代理 |
| 无 | `/em/estop_immediate` | 新增 E-Stop 紧急广播 Topic |
| 无 | `AcknowledgeEstopRelease` | 新增 E-Stop 解除确认 Service |
| MQTT（保留） | MQTT（保留） | PS ↔ HDS 通道，EM 继续使用 |
