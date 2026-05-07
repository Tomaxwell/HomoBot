---

# State Manager 模块设计

## 1. 模块概述

**模块名称**：State Manager（SM）

**定位**：SM 是端侧软件系统中的**全局状态权威**，位于中间件层。它维护机器人唯一的生命周期有限状态机（FSM），对外广播当前状态，对内校验所有状态转换请求的合法性。任何涉及运动控制的操作都必须经过 SM 状态校验，SM 是安全链的最后一道软件闸门。

**核心职责**：
1. 维护机器人全局生命周期 FSM（唯一状态源）
2. 接收并校验状态转换请求，按优先级仲裁冲突
3. 广播状态变更事件，供所有模块订阅
4. 提供急停（E-Stop）快速路径，优先级最高（priority=100）
5. 为运动类模块提供状态查询接口，阻断非法运动指令

**与相邻模块的边界**：

| 边界 | SM 负责 | 对方负责 |
|------|---------|---------|
| SM ↔ EM | 状态转换决策，广播状态 | 进程生命周期管理，启停模块 |
| SM ↔ HDS | 接受 HDS 的降级/故障转换请求 | 故障诊断与定级，不直接改状态 |
| SM ↔ MC/MS/MP | 提供状态查询，拒绝非法状态下的运动 | 运动执行，调用前校验状态 |
| SM ↔ Gateway | 接受用户侧操作指令（解除急停等） | 云端通信，转发用户操作 |
| SM ↔ TE | 接受任务引擎的状态转换请求 | 任务调度与编排 |

---

## 2. 状态机设计

### 2.1 状态枚举

| 状态 | 值 | 说明 |
|------|-----|------|
| `BOOTING` | 0 | 系统启动中，等待关键模块就绪 |
| `STANDBY` | 1 | 待机，所有关键模块就绪，可接受任务 |
| `CHARGING` | 2 | 充电中，关节锁定，运动受限 |
| `UPDATING` | 3 | 固件升级中，系统受限，禁止运动 |
| `DEBUG` | 4 | 调试模式，研发专用，可绕过部分安全校验 |
| `ACTIVE_STAND` | 5 | 站立模式，双脚站立维持平衡（默认姿态） |
| `ACTIVE_READY` | 6 | 预备模式，关节上电预紧，准备执行动作 |
| `ACTIVE_SQUAT` | 7 | 下蹲模式，降低重心 |
| `ACTIVE_SIT` | 8 | 落座模式，坐下/蹲下，重心最低 |
| `ACTIVE_MOTION` | 9 | 运动模式，执行特定动作/任务 |
| `ACTIVE_WALKING` | 10 | 持续走路模式，连续步态移动 |
| `ACTIVE_ZERO_TORQUE` | 11 | 零力矩模式，关节零力矩输出，可被动推动 |
| `ACTIVE_DAMPING` | 12 | 阻尼模式，关节提供阻尼力，缓冲外力冲击 |
| `ACTIVE_E_STOP` | 13 | 急停激活，运动已锁定，需人工解除 |
| `DEGRADED` | 14 | 降级运行，部分模块不可用但系统可控 |
| `FAULT` | 15 | 严重故障，需人工确认才能恢复 |
| `SHUTTING_DOWN` | 16 | 正在关机 |

### 2.2 状态机图

```mermaid
stateDiagram-v2
    direction TB
    [*] --> BOOTING : power_on

    state 启动与维护 {
        BOOTING --> STANDBY : all_ready
        BOOTING --> FAULT : boot_timeout(30s) / critical_fail
        BOOTING --> SHUTTING_DOWN : shutdown
        STANDBY --> CHARGING : charging_connect
        STANDBY --> UPDATING : fota_start
        STANDBY --> DEBUG : debug_enter
        STANDBY --> SHUTTING_DOWN : shutdown
        STANDBY --> FAULT : critical_fail
        STANDBY --> DEGRADED : non_critical_fail
        CHARGING --> STANDBY : charging_complete
        CHARGING --> FAULT : battery_fault
        CHARGING --> SHUTTING_DOWN : shutdown
        UPDATING --> STANDBY : update_success
        UPDATING --> FAULT : update_fail
        UPDATING --> SHUTTING_DOWN : shutdown
        DEBUG --> STANDBY : debug_exit
        DEBUG --> SHUTTING_DOWN : shutdown
    }

    state 运动状态 {
        direction LR
        [*] --> ACTIVE_STAND
        ACTIVE_STAND --> ACTIVE_READY : ready
        ACTIVE_STAND --> ACTIVE_SQUAT : squat
        ACTIVE_STAND --> ACTIVE_SIT : sit
        ACTIVE_STAND --> ACTIVE_ZERO_TORQUE : zero_torque
        ACTIVE_STAND --> ACTIVE_DAMPING : damping
        ACTIVE_READY --> ACTIVE_MOTION : motion
        ACTIVE_READY --> ACTIVE_WALKING : walking
        ACTIVE_READY --> ACTIVE_SQUAT : squat
        ACTIVE_READY --> ACTIVE_SIT : sit
        ACTIVE_MOTION --> ACTIVE_READY : motion_done
        ACTIVE_MOTION --> ACTIVE_STAND : stand
        ACTIVE_WALKING --> ACTIVE_READY : stop_walking
        ACTIVE_WALKING --> ACTIVE_STAND : stand
        ACTIVE_SQUAT --> ACTIVE_SIT : sit_down
        ACTIVE_SQUAT --> ACTIVE_READY : ready
        ACTIVE_SQUAT --> ACTIVE_STAND : stand_up
        ACTIVE_SIT --> ACTIVE_SQUAT : rise
        ACTIVE_SIT --> ACTIVE_READY : ready
        ACTIVE_SIT --> ACTIVE_STAND : stand_up
        ACTIVE_ZERO_TORQUE --> ACTIVE_DAMPING : damping
        ACTIVE_ZERO_TORQUE --> ACTIVE_READY : ready
        ACTIVE_ZERO_TORQUE --> ACTIVE_STAND : recover
        ACTIVE_DAMPING --> ACTIVE_ZERO_TORQUE : zero_torque
        ACTIVE_DAMPING --> ACTIVE_READY : ready
        ACTIVE_DAMPING --> ACTIVE_STAND : recover
    }

    state 安全与故障 {
        direction TB
        ACTIVE_E_STOP --> ACTIVE_STAND : operator_release
        ACTIVE_E_STOP --> FAULT : hardware_damage
        DEGRADED --> STANDBY : recovered
        DEGRADED --> FAULT : fault_escalate
        FAULT --> STANDBY : acknowledge_fault
        FAULT --> SHUTTING_DOWN : acknowledge_shutdown
    }

    STANDBY --> ACTIVE_STAND : activate
    ACTIVE_STAND --> STANDBY : deactivate
    运动状态 --> ACTIVE_E_STOP : e_stop(pri=100)
    运动状态 --> FAULT : critical_fail
    运动状态 --> DEGRADED : non_critical_fail
    CHARGING --> ACTIVE_E_STOP : e_stop(pri=100)
    SHUTTING_DOWN --> [*]
```

### 2.3 状态转换表

| 当前状态 | 目标状态 | 触发条件 | 触发者 | 优先级 |
|----------|----------|----------|--------|--------|
| `BOOTING` | `STANDBY` | 所有 P0/P1 模块就绪 | EM | 60 |
| `BOOTING` | `FAULT` | 启动超时（30s）或关键模块失败 | SM 内部 / EM | 80 |
| `BOOTING` | `SHUTTING_DOWN` | 启动过程中收到关机指令 | Gateway | 60 |
| `STANDBY` | `ACTIVE_STAND` | 激活运动控制（站立姿态） | TE / Gateway | 40 |
| `STANDBY` | `CHARGING` | 检测到充电连接 | HDS | 60 |
| `STANDBY` | `UPDATING` | FOTA 升级开始 | FOTA | 60 |
| `STANDBY` | `DEBUG` | 研发人员进入调试模式 | Gateway（operator） | 20 |
| `STANDBY` | `SHUTTING_DOWN` | 关机指令 | Gateway / TE | 60 |
| `STANDBY` | `FAULT` | 关键模块故障 | HDS | 80 |
| `STANDBY` | `DEGRADED` | 非关键模块故障 | HDS | 80 |
| `CHARGING` | `STANDBY` | 充电完成 / 拔出充电桩 | HDS | 60 |
| `CHARGING` | `FAULT` | 充电中电池故障/过热 | HDS | 80 |
| `CHARGING` | `SHUTTING_DOWN` | 充电中关机 | Gateway | 60 |
| `UPDATING` | `STANDBY` | 升级成功重启完成 | FOTA | 60 |
| `UPDATING` | `FAULT` | 升级失败 | FOTA | 80 |
| `UPDATING` | `SHUTTING_DOWN` | 升级中强制关机 | Gateway | 60 |
| `DEBUG` | `STANDBY` | 退出调试模式 | Gateway（operator） | 20 |
| `DEBUG` | `SHUTTING_DOWN` | 调试中关机 | Gateway | 60 |
| `ACTIVE_STAND` | `ACTIVE_READY` | 预备（关节上电预紧） | TE / MC | 40 |
| `ACTIVE_STAND` | `ACTIVE_SQUAT` | 下蹲指令 | TE / Gateway | 40 |
| `ACTIVE_STAND` | `ACTIVE_SIT` | 坐下指令 | TE / Gateway | 40 |
| `ACTIVE_STAND` | `ACTIVE_ZERO_TORQUE` | 进入零力矩模式 | Gateway（operator） | 20 |
| `ACTIVE_STAND` | `ACTIVE_DAMPING` | 进入阻尼模式 | Gateway（operator） | 20 |
| `ACTIVE_STAND` | `STANDBY` | 去激活运动控制 | TE / Gateway | 40 |
| `ACTIVE_STAND` | `ACTIVE_E_STOP` | 急停触发 | MC, EM, HDS, Gateway, hardware_estop | 100 |
| `ACTIVE_STAND` | `FAULT` | 关键故障 | HDS | 80 |
| `ACTIVE_STAND` | `DEGRADED` | 非关键模块故障 | HDS | 80 |
| `ACTIVE_READY` | `ACTIVE_STAND` | 回到站立 | TE / MC | 40 |
| `ACTIVE_READY` | `ACTIVE_MOTION` | 开始执行动作/任务 | TE | 40 |
| `ACTIVE_READY` | `ACTIVE_WALKING` | 开始持续走路 | TE / PnC | 40 |
| `ACTIVE_READY` | `ACTIVE_SQUAT` | 下蹲指令 | TE / Gateway | 40 |
| `ACTIVE_READY` | `ACTIVE_SIT` | 坐下指令 | TE / Gateway | 40 |
| `ACTIVE_READY` | `ACTIVE_E_STOP` | 急停触发 | MC, EM, HDS, Gateway, hardware_estop | 100 |
| `ACTIVE_READY` | `FAULT` | 关键故障 | HDS | 80 |
| `ACTIVE_READY` | `DEGRADED` | 非关键模块故障 | HDS | 80 |
| `ACTIVE_MOTION` | `ACTIVE_READY` | 动作完成 / 取消 | TE | 40 |
| `ACTIVE_MOTION` | `ACTIVE_STAND` | 回到站立 | TE / MC | 40 |
| `ACTIVE_MOTION` | `ACTIVE_E_STOP` | 急停触发 | MC, EM, HDS, Gateway, hardware_estop | 100 |
| `ACTIVE_MOTION` | `FAULT` | 关键故障 | HDS | 80 |
| `ACTIVE_MOTION` | `DEGRADED` | 非关键模块故障 | HDS | 80 |
| `ACTIVE_WALKING` | `ACTIVE_READY` | 停止走路 | TE / PnC | 40 |
| `ACTIVE_WALKING` | `ACTIVE_STAND` | 回到站立 | TE / PnC | 40 |
| `ACTIVE_WALKING` | `ACTIVE_E_STOP` | 急停触发 | MC, EM, HDS, Gateway, hardware_estop | 100 |
| `ACTIVE_WALKING` | `FAULT` | 关键故障 | HDS | 80 |
| `ACTIVE_WALKING` | `DEGRADED` | 非关键模块故障 | HDS | 80 |
| `ACTIVE_SQUAT` | `ACTIVE_STAND` | 起立 | TE / Gateway | 40 |
| `ACTIVE_SQUAT` | `ACTIVE_SIT` | 继续坐下 | TE / Gateway | 40 |
| `ACTIVE_SQUAT` | `ACTIVE_READY` | 预备 | TE / MC | 40 |
| `ACTIVE_SQUAT` | `ACTIVE_E_STOP` | 急停触发 | MC, EM, HDS, Gateway, hardware_estop | 100 |
| `ACTIVE_SQUAT` | `FAULT` | 关键故障 | HDS | 80 |
| `ACTIVE_SQUAT` | `DEGRADED` | 非关键模块故障 | HDS | 80 |
| `ACTIVE_SIT` | `ACTIVE_SQUAT` | 起身（过渡） | TE / Gateway | 40 |
| `ACTIVE_SIT` | `ACTIVE_STAND` | 直接起立 | TE / Gateway | 40 |
| `ACTIVE_SIT` | `ACTIVE_READY` | 预备 | TE / MC | 40 |
| `ACTIVE_SIT` | `ACTIVE_E_STOP` | 急停触发 | MC, EM, HDS, Gateway, hardware_estop | 100 |
| `ACTIVE_SIT` | `FAULT` | 关键故障 | HDS | 80 |
| `ACTIVE_SIT` | `DEGRADED` | 非关键模块故障 | HDS | 80 |
| `ACTIVE_ZERO_TORQUE` | `ACTIVE_STAND` | 恢复控制 | Gateway（operator） | 20 |
| `ACTIVE_ZERO_TORQUE` | `ACTIVE_DAMPING` | 切换阻尼模式 | Gateway（operator） | 20 |
| `ACTIVE_ZERO_TORQUE` | `ACTIVE_READY` | 恢复预备 | Gateway（operator） | 20 |
| `ACTIVE_ZERO_TORQUE` | `ACTIVE_E_STOP` | 急停触发 | MC, EM, HDS, Gateway, hardware_estop | 100 |
| `ACTIVE_ZERO_TORQUE` | `FAULT` | 关键故障 | HDS | 80 |
| `ACTIVE_DAMPING` | `ACTIVE_STAND` | 恢复控制 | Gateway（operator） | 20 |
| `ACTIVE_DAMPING` | `ACTIVE_ZERO_TORQUE` | 切换零力矩模式 | Gateway（operator） | 20 |
| `ACTIVE_DAMPING` | `ACTIVE_READY` | 恢复预备 | Gateway（operator） | 20 |
| `ACTIVE_DAMPING` | `ACTIVE_E_STOP` | 急停触发 | MC, EM, HDS, Gateway, hardware_estop | 100 |
| `ACTIVE_DAMPING` | `FAULT` | 关键故障 | HDS | 80 |
| `ACTIVE_E_STOP` | `ACTIVE_STAND` | 人工确认解除急停 | Gateway（operator） | 20 |
| `ACTIVE_E_STOP` | `FAULT` | 急停后检测到硬件损坏 | HDS | 80 |
| `DEGRADED` | `STANDBY` | 故障模块恢复 | HDS / EM | 60 |
| `DEGRADED` | `FAULT` | 故障升级 | HDS | 80 |
| `FAULT` | `STANDBY` | 人工确认故障 + 恢复前检查通过 | Gateway（operator） | 20 |
| `FAULT` | `SHUTTING_DOWN` | 人工选择关机 | Gateway（operator） | 20 |

### 2.4 状态转换约束

1. **E-Stop 不可被软件自动解除** — `ACTIVE_E_STOP → ACTIVE_STAND` 必须由人工通过 Gateway 确认，需提供 `operator_id`
2. **FAULT 不可自动恢复** — 必须经过 `AcknowledgeFault` Service，需提供 `operator_id`
3. **BOOTING 超时必须进 FAULT** — 不允许超时后进入 `STANDBY`
4. **E-Stop 优先级最高** — priority=100，任何 ACTIVE_* 状态（除 ACTIVE_E_STOP 自身）及 CHARGING 状态下收到 E-Stop 请求立即响应；UPDATING/DEBUG 状态下 E-Stop 转为 FAULT
5. **SHUTTING_DOWN 是终态** — 任意状态（含 BOOTING）均可转换至 SHUTTING_DOWN；进入后不可回退，只能等待 EM 关闭所有进程
6. **CHARGING 禁止主动运动** — CHARGING 状态下 `IsMotionAllowed` 固定返回 `allowed=false`
7. **UPDATING 禁止所有运动** — UPDATING 状态下 `IsMotionAllowed` 固定返回 `allowed=false`
8. **DEBUG 模式运动需特殊授权** — DEBUG 状态下运动指令需附加 `debug_token`，否则拒绝

---

## 3. ROS2 接口定义

### 3.1 消息定义（msg）

```
# sm_msgs/msg/RobotState.msg
# 机器人全局状态广播消息

# 状态枚举
uint8 BOOTING            = 0
uint8 STANDBY            = 1
uint8 CHARGING           = 2
uint8 UPDATING           = 3
uint8 DEBUG              = 4
uint8 ACTIVE_STAND       = 5
uint8 ACTIVE_READY       = 6
uint8 ACTIVE_SQUAT       = 7
uint8 ACTIVE_SIT         = 8
uint8 ACTIVE_MOTION      = 9
uint8 ACTIVE_WALKING     = 10
uint8 ACTIVE_ZERO_TORQUE = 11
uint8 ACTIVE_DAMPING     = 12
uint8 ACTIVE_E_STOP      = 13
uint8 DEGRADED           = 14
uint8 FAULT              = 15
uint8 SHUTTING_DOWN      = 16

# 辅助字段（便于订阅者快速判断，不用解析 state）
bool is_active                     # 当前是否处于某种 ACTIVE_* 模式（除 ACTIVE_E_STOP）
bool is_motion_allowed             # 当前是否允许运动指令（所有 ACTIVE_* 除 E_STOP 为 true）

uint8 state                        # 当前主状态
uint8 previous_state               # 上一个状态
builtin_interfaces/Time entered_at # 进入当前状态的时间戳
uint32 transition_count            # 累计转换次数
string active_task_id              # 当前正在执行的任务 ID（ACTIVE_MOTION / ACTIVE_WALKING 时有值）
string last_transition_requester   # 最近一次转换的请求者节点名
string last_transition_reason      # 最近一次转换原因
```

```
# sm_msgs/msg/TransitionEvent.msg
# 状态转换事件（详细记录）

uint8 from_state                   # 转换前状态
uint8 to_state                     # 转换后状态
builtin_interfaces/Time stamp      # 转换发生时间
string requester_node              # 请求者节点名称
string reason                      # 转换原因描述
int32 priority                     # 请求优先级
bool accepted                      # 是否被接受
string reject_reason               # 拒绝原因（accepted=false 时有值）
```

```
# sm_msgs/msg/Heartbeat.msg
# SM 模块心跳消息

builtin_interfaces/Time stamp
string node_name                   # 节点名称（固定 "state_manager"）
uint8 state                        # 当前机器人状态
bool healthy                       # FSM 引擎是否正常
string status_message              # 状态描述
uint32 pending_requests            # 排队中的转换请求数
```

```
# sm_msgs/msg/ErrorCode.msg
# SM 模块错误码定义（原名 SmErrorCode.msg，应改为 ErrorCode.msg）

# 错误码枚举
uint16 OK                               = 0
uint16 ERR_SM_INVALID_TRANSITION        = 1001   # 状态转换不合法（FSM 不允许）
uint16 ERR_SM_PRIORITY_TOO_LOW          = 1002   # 请求优先级低于当前锁定优先级
uint16 ERR_SM_ESTOP_ACTIVE              = 1003   # 急停状态下拒绝运动相关转换
uint16 ERR_SM_FAULT_UNACKNOWLEDGED      = 1004   # FAULT 状态未经人工确认
uint16 ERR_SM_OPERATOR_ID_REQUIRED      = 1005   # 需要操作者 ID（解除急停/确认故障）
uint16 ERR_SM_PRECONDITION_FAILED       = 1006   # 前置条件不满足（恢复前检查失败）
uint16 ERR_SM_ALREADY_IN_STATE          = 1007   # 已经处于目标状态
uint16 ERR_SM_SHUTTING_DOWN             = 1008   # 系统正在关机，拒绝所有请求
uint16 ERR_SM_BOOT_TIMEOUT              = 1009   # 启动超时
uint16 ERR_SM_CONCURRENT_TRANSITION     = 1010   # 正在处理另一个转换请求

uint16 error_code                       # 错误码
string message                          # 可读错误描述
```

### 3.2 服务定义（srv）

```
# sm_msgs/srv/RequestTransition.srv
# 请求状态转换

# Request
uint8 target_state                  # 目标状态（使用 RobotState 枚举）
int32 priority                      # 请求优先级（参见优先级规范）
string requester_node               # 请求者节点名称
string reason                       # 转换原因
string task_id                      # 关联的任务 ID（可选）
---
# Response
bool accepted                       # 是否被接受
uint16 error_code                   # 错误码（ErrorCode 枚举）
string message                      # 可读结果描述
uint8 current_state                 # 当前实际状态
```

```
# sm_msgs/srv/GetState.srv
# 查询当前状态

# Request（空）
---
# Response
uint8 state                         # 当前主状态
builtin_interfaces/Time entered_at  # 进入当前状态的时间戳
uint32 transition_count             # 累计转换次数
string active_task_id               # 当前任务 ID
```

```
# sm_msgs/srv/TriggerEStop.srv
# 触发急停（优先级固定 100，不可覆盖）

# Request
string requester_node               # 请求者节点名称
string reason                       # 急停原因
---
# Response
bool accepted                       # 是否被接受（仅 SHUTTING_DOWN 时拒绝）
uint16 error_code                   # 错误码
string message                      # 结果描述
```

```
# sm_msgs/srv/ReleaseEStop.srv
# 解除急停（必须人工操作）

# Request
string operator_id                  # 操作者 ID（必填）
string reason                       # 解除原因
---
# Response
bool accepted                       # 是否被接受
uint16 error_code                   # 错误码
string message                      # 结果描述
uint8 current_state                 # 解除后的当前状态
```

```
# sm_msgs/srv/AcknowledgeFault.srv
# 确认故障（从 FAULT 恢复）

# Request
string operator_id                  # 操作者 ID（必填）
bool shutdown_instead               # true = 关机而非恢复
---
# Response
bool accepted                       # 是否被接受
uint16 error_code                   # 错误码
string message                      # 结果描述（含恢复前检查结果）
uint8 current_state                 # 确认后的当前状态
```

```
# sm_msgs/srv/IsMotionAllowed.srv
# 运动模块调用：查询当前是否允许运动指令
# 允许：ACTIVE_STAND / ACTIVE_READY / ACTIVE_SQUAT / ACTIVE_SIT / ACTIVE_MOTION / ACTIVE_WALKING / ACTIVE_ZERO_TORQUE / ACTIVE_DAMPING
# 拒绝：其他所有状态（含 ACTIVE_E_STOP）

# Request（空）
---
# Response
bool allowed                        # 是否允许
uint8 current_state                 # 当前状态
uint16 error_code                   # 不允许时的错误码
string message                      # 不允许时的原因
```

```
# sm_msgs/srv/GetHealthStatus.srv
# SM 模块健康状态查询

---
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
uint32 total_transitions            # 总转换次数
uint32 rejected_transitions         # 拒绝的转换次数
```

### 3.3 接口汇总表

#### Topics

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/sm/robot_state` | `sm_msgs/msg/RobotState` | SM → ALL | Reliable + Transient Local + Depth 1 | 事件驱动（状态变更时发布） | 机器人全局状态广播 |
| `/sm/transition_event` | `sm_msgs/msg/TransitionEvent` | SM → ALL | Reliable + Volatile + Depth 50 | 事件驱动 | 状态转换事件日志 |
| `/sm/heartbeat` | `sm_msgs/msg/Heartbeat` | SM → EM/HDS | Reliable + Volatile + Depth 1 | 1 Hz | SM 心跳 |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/sm/request_transition` | `sm_msgs/srv/RequestTransition` | EM, HDS, TE, Gateway | 请求状态转换 |
| `/sm/get_state` | `sm_msgs/srv/GetState` | 任意模块 | 查询当前状态 |
| `/sm/trigger_estop` | `sm_msgs/srv/TriggerEStop` | 任意模块 | 触发急停（priority=100） |
| `/sm/release_estop` | `sm_msgs/srv/ReleaseEStop` | Gateway（人工） | 解除急停 |
| `/sm/acknowledge_fault` | `sm_msgs/srv/AcknowledgeFault` | Gateway（人工） | 确认故障 |
| `/sm/is_motion_allowed` | `sm_msgs/srv/IsMotionAllowed` | MC, MS, MP, PnC | 运动前状态校验 |
| `/sm/get_health_status` | `sm_msgs/srv/GetHealthStatus` | EM, HDS | SM 健康查询 |

---

## 4. 内部设计

### 4.1 节点架构

```mermaid
flowchart TB
    subgraph SMNode["StateManagerNode"]
        FSM["FSM Engine
(状态机核心)
· 状态存储 / 转换执行 / 事件发布"]
        Validator["Transition Validator
(转换校验器)
· 合法性校验 / 优先级仲裁 / 前置条件检查"]
        Queue["Transition Queue
(优先级队列，高优先级抢占)"]
        Publisher["State Publisher
· /sm/robot_state (Transient Local)
· /sm/transition_event"]
        EStop["E-Stop Handler
(急停快速路径)
· 独立回调组"]
        Heartbeat["Heartbeat Timer (1Hz)"]
        Watchdog["Boot Watchdog
(启动超时监控)"]
        Config["Config Manager
(参数管理)"]

        FSM --> Queue
        Validator --> Queue
        Queue --> Publisher
        EStop --> FSM
    end
```

### 4.2 关键设计决策

1. **E-Stop 独立回调组**：`TriggerEStop` Service 使用独立的 `MutuallyExclusive` CallbackGroup，确保即使其他 Service 正在执行，E-Stop 请求也能被立即处理
2. **优先级队列**：多个转换请求同时到达时，按 priority 降序处理；同优先级按到达时间
3. **原子性转换**：状态转换在单次回调中完成（先校验、再切换、最后发布），无中间态暴露

### 4.3 关键流程

#### 4.3.1 系统启动流程

```
EM 启动 SM 进程
  → SM 初始化，状态设为 BOOTING
  → 发布 /sm/robot_state (BOOTING)
  → 启动 Boot Watchdog 定时器（30s）
  → 等待 EM 通过 /sm/request_transition 通知 all_ready
  → Validator 检查转换合法性（BOOTING → STANDBY）
  → FSM 执行转换，发布新状态
  → 取消 Boot Watchdog
  → 若 30s 超时仍未收到 → 自动转入 FAULT
```

#### 4.3.2 E-Stop 触发流程

```
白名单模块（MC, EM, HDS, Gateway, hardware_estop）调用 /sm/trigger_estop
  → E-Stop Handler 收到请求（独立回调组，不阻塞）
  → 检查当前状态：
      - ACTIVE_STAND / ACTIVE_READY / ACTIVE_SQUAT / ACTIVE_SIT / ACTIVE_MOTION / ACTIVE_WALKING / ACTIVE_ZERO_TORQUE / ACTIVE_DAMPING / DEGRADED → 立即转为 ACTIVE_E_STOP
      - ACTIVE_E_STOP → 已是急停，返回 accepted=true
      - CHARGING → 断开充电回路，转为 ACTIVE_E_STOP
      - FAULT / SHUTTING_DOWN → 返回 accepted=false (已在更高级别保护)
      - BOOTING / STANDBY / UPDATING / DEBUG → 转为 FAULT（无运动上下文或不宜中断）
  → 发布 /sm/robot_state (ACTIVE_E_STOP)
  → 发布 /sm/transition_event（记录完整审计日志）
  → MC/MS/MP 订阅 /sm/robot_state，收到后立即锁定运动
```

#### 4.3.3 故障恢复流程

```
操作者通过 Gateway 调用 /sm/acknowledge_fault
  → Validator 校验：
      1. 当前状态必须是 FAULT
      2. operator_id 不为空
      3. shutdown_instead 决定恢复路径
  → 若 shutdown_instead=true → FAULT → SHUTTING_DOWN
  → 若 shutdown_instead=false：
      → SM 调用 EM /em/get_process_status 检查关键进程状态
      → SM 调用 HDS 检查硬件健康
      → 全部通过 → FAULT → STANDBY
      → 检查失败 → 返回 accepted=false, ERR_PRECONDITION_FAILED
```

#### 4.3.4 状态转换通用流程

```
调用者 → /sm/request_transition(target_state, priority, requester, reason)
  → Transition Validator:
      1. 当前是否为 SHUTTING_DOWN → 拒绝 (ERR_SHUTTING_DOWN)
      2. 是否正在处理另一个转换 → 比较优先级，高优先级抢占
      3. FSM 转换表是否允许 from→to → 不允许则拒绝 (ERR_INVALID_TRANSITION)
      4. 特殊约束检查（E-Stop 解除需 operator_id 等）
  → FSM Engine 执行转换
  → State Publisher 发布 /sm/robot_state + /sm/transition_event
  → 返回 accepted=true
```

---

## 5. 与其他模块的交互

### 5.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|------|------|------|------|
| EM | EM → SM | `/sm/request_transition` (Service) | EM 通知模块就绪（BOOTING→STANDBY）、模块崩溃触发降级/故障 |
| EM | SM → EM | `/sm/robot_state` (Topic) | EM 订阅状态，按状态调整进程组（如 STANDBY 时启动 perception/control） |
| HDS | HDS → SM | `/sm/request_transition` (Service) | HDS 请求降级（→DEGRADED）或故障（→FAULT） |
| HDS | SM → HDS | `/sm/robot_state` (Topic) | HDS 订阅状态用于健康聚合 |
| Gateway | Gateway → SM | `/sm/request_transition` (Service) | 转发用户激活/待机指令 |
| Gateway | Gateway → SM | `/sm/release_estop` (Service) | 转发人工解除急停 |
| Gateway | Gateway → SM | `/sm/acknowledge_fault` (Service) | 转发人工确认故障 |
| Gateway | SM → Gateway | `/sm/robot_state` (Topic) | Gateway 将状态同步到云端/APP |
| TE | TE → SM | `/sm/request_transition` (Service) | 任务开始（→ACTIVE_MOTION/ACTIVE_WALKING）、任务结束（→ACTIVE_READY/ACTIVE_STAND） |
| MC | MC → SM | `/sm/is_motion_allowed` (Service) | 运动指令执行前校验 |
| MC | SM → MC | `/sm/robot_state` (Topic) | MC 订阅状态，E-Stop 时锁定关节 |
| MS | MS → SM | `/sm/is_motion_allowed` (Service) | 运动流指令执行前校验 |
| MS | SM → MS | `/sm/robot_state` (Topic) | MS 订阅状态 |
| MP | MP → SM | `/sm/is_motion_allowed` (Service) | 动作播放前校验 |
| MP | SM → MP | `/sm/robot_state` (Topic) | MP 订阅状态 |
| PnC | PnC → SM | `/sm/is_motion_allowed` (Service) | 导航规划前校验 |
| PnC | SM → PnC | `/sm/robot_state` (Topic) | PnC 订阅状态 |
| DR | SM → DR | `/sm/transition_event` (Topic) | DR 记录所有状态转换事件用于回放分析 |
| FOTA | FOTA → SM | `/sm/request_transition` (Service) | OTA 升级前请求进入 STANDBY |

### 5.2 关键交互时序

#### 正常任务执行时序

```mermaid
sequenceDiagram
    participant Gateway
    participant TE
    participant SM
    participant MC
    participant MP
    Gateway->>SM: activate
    TE->>SM: RequestTransition (ACTIVE_STAND, pri=20)
    SM-->>TE: accepted
    SM->>TE: RobotState=ACTIVE_STAND
    SM->>MC: RobotState=ACTIVE_STAND
    SM->>MP: RobotState=ACTIVE_STAND
    Gateway->>TE: start_task
    TE->>SM: RequestTransition (ACTIVE_MOTION, pri=40)
    SM-->>TE: accepted
    MC->>SM: IsMotionAllowed
    SM-->>MC: allowed
    MC->>MP: execute
```

---

## 6. 关键参数与配置

```yaml
# sm/config/sm_params.yaml

state_manager:
  ros__parameters:
    # 启动超时（秒），超时则进入 FAULT
    boot_timeout_sec: 30.0

    # 心跳发布频率（Hz）
    heartbeat_rate_hz: 1.0

    # 状态广播 QoS 深度
    state_qos_depth: 1

    # 转换事件 QoS 深度
    transition_event_qos_depth: 50

    # E-Stop 优先级（固定值，不可修改）
    estop_priority: 100

    # 转换请求超时（秒），超时自动丢弃排队请求
    transition_request_timeout_sec: 5.0

    # 最大排队转换请求数
    max_pending_requests: 10

    # 恢复前检查超时（秒）
    recovery_check_timeout_sec: 10.0

    # 转换事件日志保留条数（内存中）
    transition_history_size: 1000

    # 是否启用详细转换日志
    verbose_transition_log: true
```

---

## 7. 错误码定义

| 错误码 | 常量名 | 说明 |
|--------|--------|------|
| 0 | `OK` | 成功 |
| 1001 | `ERR_SM_INVALID_TRANSITION` | FSM 转换表中不存在该转换路径 |
| 1002 | `ERR_SM_PRIORITY_TOO_LOW` | 请求优先级低于正在执行的转换 |
| 1003 | `ERR_SM_ESTOP_ACTIVE` | 当前处于急停状态，拒绝运动相关操作 |
| 1004 | `ERR_SM_FAULT_UNACKNOWLEDGED` | 当前处于 FAULT 状态，需人工确认 |
| 1005 | `ERR_SM_OPERATOR_ID_REQUIRED` | 解除急停/确认故障需提供 operator_id |
| 1006 | `ERR_SM_PRECONDITION_FAILED` | 恢复前置条件不满足（硬件/进程异常） |
| 1007 | `ERR_SM_ALREADY_IN_STATE` | 已处于目标状态，无需转换 |
| 1008 | `ERR_SM_SHUTTING_DOWN` | 系统正在关机，拒绝所有请求 |
| 1009 | `ERR_SM_BOOT_TIMEOUT` | 启动超时，已自动进入 FAULT |
| 1010 | `ERR_SM_CONCURRENT_TRANSITION` | 正在处理另一个转换，且优先级不足以抢占 |
| 1011 | `ERR_SM_CHARGING` | 当前处于充电状态，禁止运动 |
| 1012 | `ERR_SM_UPDATING` | 当前处于升级状态，禁止运动 |
| 1013 | `ERR_SM_DEBUG_MODE` | 当前处于调试模式，运动需特殊授权 |
| 1014 | `ERR_SM_ZERO_TORQUE_MODE` | 当前处于零力矩模式，拒绝主动运动指令 |
| 1015 | `ERR_SM_DAMPING_MODE` | 当前处于阻尼模式，拒绝主动运动指令 |

---

## 8. 安全约束

### 8.1 E-Stop 快速路径

- `/sm/trigger_estop` 使用独立 CallbackGroup，不与其他 Service 共享线程
- E-Stop 请求处理延迟目标 < 5ms（纯状态切换，无外部调用）
- E-Stop priority 固定为 100，硬编码，不可通过参数修改
- E-Stop 可从所有 `ACTIVE_*` 状态（除 `ACTIVE_E_STOP`）、`CHARGING`、`DEGRADED` 触发

### 8.2 FAULT 状态行为

- 进入 FAULT 后，`/sm/is_motion_allowed` 固定返回 `allowed=false`
- FAULT 状态只能通过 `AcknowledgeFault` Service 退出
- 退出 FAULT 前必须执行恢复前检查（查询 EM 进程状态 + HDS 硬件状态）
- FAULT 期间，SM 继续发布心跳和状态广播，不自行退出

### 8.3 ACTIVE_E_STOP 状态行为

- 所有运动指令被拒绝（`IsMotionAllowed` 返回 false + `ERR_ESTOP_ACTIVE`）
- 不允许软件自动解除：超时不解除、任务完成不解除
- 解除必须通过 `/sm/release_estop`，且 `operator_id` 不为空
- 解除后进入 `ACTIVE_STAND`（非 `ACTIVE_READY`），需要重新预备才能执行动作或走路

### 8.4 审计日志

- 所有转换请求（无论接受/拒绝）都记录到 `/sm/transition_event`
- 记录字段：时间戳、请求者节点、原因、优先级、接受/拒绝、拒绝原因
- DR 模块订阅该 Topic 持久化存储

---

## 9. 包结构

```
sm_msgs/
    msg/
        RobotState.msg              # 机器人全局状态
        TransitionEvent.msg         # 状态转换事件
        Heartbeat.msg               # SM 心跳
        ErrorCode.msg               # 错误码定义（原名 SmErrorCode.msg）
    srv/
        RequestTransition.srv       # 请求状态转换
        GetState.srv                # 查询当前状态
        TriggerEStop.srv            # 触发急停
        ReleaseEStop.srv            # 解除急停
        AcknowledgeFault.srv        # 确认故障
        IsMotionAllowed.srv         # 运动前校验
        GetHealthStatus.srv         # 健康状态查询
    CMakeLists.txt
    package.xml

sm/
    include/sm/
        state_manager_node.hpp      # 主节点类
        fsm_engine.hpp              # FSM 引擎（状态存储 + 转换执行）
        transition_validator.hpp    # 转换校验器（合法性 + 优先级 + 前置条件）
        estop_handler.hpp           # 急停快速路径处理
        boot_watchdog.hpp           # 启动超时监控
    src/
        state_manager_node.cpp
        fsm_engine.cpp
        transition_validator.cpp
        estop_handler.cpp
        boot_watchdog.cpp
    test/
        test_fsm_engine.cpp         # FSM 转换表单元测试
        test_transition_validator.cpp # 校验器单元测试
        test_estop_handler.cpp      # E-Stop 路径测试
        test_integration.cpp        # 集成测试（多模块交互）
    config/
        sm_params.yaml              # 参数配置
    launch/
        sm.launch.py                # Launch 文件
    CMakeLists.txt
    package.xml
```

---

## 10. 关键性能指标（KPI）

| 指标 | 目标值 |
|------|--------|
| E-Stop 请求处理延迟 | < 5ms |
| 普通状态转换处理延迟 | < 10ms |
| `/sm/is_motion_allowed` 响应延迟 | < 1ms |
| `/sm/robot_state` 发布延迟（转换完成到发布） | < 1ms |
| SM 心跳抖动 | < 50ms |
| SM 自身 CPU 占用 | < 0.5% |
| SM 自身内存占用 | < 30MB |
| 系统启动到 STANDBY 状态 | < 30s（由 boot_timeout_sec 约束） |
