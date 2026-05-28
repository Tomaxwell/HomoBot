# HDS Master 模块设计

> **创建日期**：2026-05-27
> **前置文档**：hds_design.md（单 SOC 原始设计，本文档为多 SOC 架构重构后的 Master 侧设计）
> **关联文档**：hds_slave_design.md（Slave 侧设计）

---

## 1. 模块概述与定位

**模块名称**：HDS Master（Health Diagnosis System — Master）

**定位**：HDS Master 是端侧软件系统中的**全局故障诊断与定级权威**，位于中间件层。它运行在主控 SOC（SOC-0）上，与 SM 共置，汇聚本地模块和所有远端 HDS Slave 上报的健康数据，执行多维诊断分析与跨 SOC 关联诊断，做出全局故障定级决策，并通过 SM 接口请求状态转换。

**核心职责**：

1. **全局健康数据汇聚**：接收 SOC-0 本地模块直接上报 + 各远端 Slave 批量转发的健康数据
2. **Slave 生命周期管理**：维护 Slave 注册表，监控 Slave 心跳，检测 Slave 断联/自治
3. **跨 SOC 关联诊断**：在单实体规则基础上增加跨 SOC 关联规则（如 SOC-1 EtherCAT 故障 + SOC-0 MC 心跳超时 → E-Stop）
4. **全局故障定级决策**：唯一有权将诊断结果定级为 `HEALTHY` / `WARNING` / `DEGRADED` / `FAULT` 的模块
5. **告警管理**：向 Gateway 上报故障告警，支持告警去重、抑制（安全关键告警不可抑制）、恢复通知
6. **恢复前检查**：响应 SM 的恢复请求，检查本地诊断数据 + 所有 Slave 连接状态
7. **诊断日志持久化**：记录完整诊断链路和定级决策依据
8. **MQTT Bridge**：通过 MQTT Broker 实现 Master-Slave 通信（复用系统现有 MQTT 基础设施）

**单 SOC 兼容**：当无 Slave 注册时，Master 自动退化为等效单 SOC HDS，所有 ROS2 接口行为与原 hds_design.md 完全一致。

**与相邻模块的边界**：

| 边界 | HDS Master 负责 | 对方负责 |
|------|-----------------|---------|
| Master ↔ SM | 请求状态转换（DEGRADED/FAULT/E-Stop）；响应恢复前检查 | 全局状态机决策与仲裁；执行状态转换 |
| Master ↔ EM（SOC-0） | 接收进程状态、L3/L4 建议（MQTT） | 进程生命周期治理；L1/L2 自主恢复 |
| Master ↔ Gateway | 上报故障告警、定级结果 | 云端/APP 通信；告警展示 |
| Master ↔ Slave | 全局定级决策、规则下发、状态广播 | 本地健康数据采集、预过滤、自治保护 |
| Master ↔ DR | 输出诊断链路记录 | 数据持久化存储 |

---

## 2. 职责边界

### 2.1 Master vs. Slave 职责划分

| 职责 | Master | Slave |
|------|--------|-------|
| 全局故障定级 | ✅ 唯一权威 | ❌ 只做本地预过滤 |
| 请求 SM 状态转换 | ✅ 通过 `/sm/request_transition` | ❌ 不与 SM 通信 |
| 触发 E-Stop | ✅ 通过 `/sm/trigger_estop` | ✅ 仅在自治模式下触发本地硬件 E-Stop |
| 告警管理 | ✅ 全局告警去重/抑制/升级 | ❌ 无告警管理 |
| 恢复前检查 | ✅ 聚合本地 + Slave 状态 | ✅ 响应 Master 查询本地状态 |
| 跨 SOC 关联诊断 | ✅ 有全局视野 | ❌ 无跨 SOC 数据 |
| 本地健康数据采集 | ✅ SOC-0 本地模块 | ✅ 所在 SOC 本地模块 |
| 数据缓冲与回放 | ❌ | ✅ Master 断联期间缓冲并在重连后回放 |
| MQTT 通信 | ✅ 订阅端（全局汇聚） | ✅ 发布端（数据上报） |

### 2.2 HDS Master 红线

- **不直接修改 SM 状态** — 通过 Service 请求，SM 仲裁后执行
- **不直接控制运动** — 不下发任何关节/运动指令
- **不启停进程** — 进程控制由 EM 负责
- **不直连云端** — 告警通过 Gateway 转发
- **不做业务决策** — 不决定任务执行策略
- **不越权管理 Slave 本地模块** — Slave 的本地采集和注册由 Slave 自主管理
- **不代替 Slave 执行本地 E-Stop** — Slave 自治模式下的本地 E-Stop 由 Slave 自主执行

---

## 3. 状态机设计

### 3.1 实体诊断状态（与原设计一致）

| 状态 | 值 | 说明 |
|------|-----|------|
| `HEALTHY` | 0 | 健康，无异常 |
| `WARNING` | 1 | 警告，存在轻微异常但可继续运行 |
| `DEGRADED` | 2 | 降级，功能受限但仍可控 |
| `FAULT` | 3 | 严重故障，需人工介入 |
| `UNKNOWN` | 4 | 数据缺失，无法判定 |

### 3.2 Slave 连接状态（Master 维护）

| 状态 | 值 | 说明 |
|------|-----|------|
| `CONNECTED` | 0 | Slave 正常连接，心跳正常 |
| `HEARTBEAT_MISS` | 1 | 心跳丢失 1-2 次，尚未超时 |
| `DISCONNECTED` | 2 | Slave 断联（心跳超时 ≥ 3 次） |
| `AUTONOMOUS` | 3 | Slave 通告已进入自治模式 |
| `RECONNECTING` | 4 | Slave 正在重连并回放缓冲数据 |

### 3.3 Slave 连接状态流转



<whiteboard type="blank"></whiteboard>



### 3.4 Slave 断联对全局定级的影响

| Slave 连接状态 | Slave 管辖实体的定级处理 | 是否请求 SM 转换 |
|---------------|------------------------|-----------------|
| `CONNECTED` | 按正常诊断规则定级 | 按规则 |
| `HEARTBEAT_MISS` | 保持上次定级，标记 `data_stale=true` | 否（暂不升级） |
| `DISCONNECTED` | **关键实体** → `FAULT`；非关键实体 → `UNKNOWN` | 关键实体时请求 |
| `AUTONOMOUS` | 由 Slave 本地处理安全，Master 标记 `UNKNOWN`；Slave 自行 E-Stop | Master 不重复请求 |
| `RECONNECTING` | 等待回放数据后重新评估 | 否（等待回放完成） |

### 3.5 诊断状态流转（与原设计一致，修复审查问题）

| 当前状态 | 目标状态 | 触发条件 | 自动/人工 |
|----------|----------|----------|-----------|
| `HEALTHY` | `WARNING` | 规则引擎检测到轻微异常 | 自动 |
| `WARNING` | `HEALTHY` | 异常指标恢复正常，持续 3 个周期 | 自动 |
| `WARNING` | `DEGRADED` | 异常升级或多维异常叠加，持续 2 个诊断周期 | 自动 |
| `DEGRADED` | `HEALTHY` | 全部异常恢复，持续 5 个周期 | 自动 |
| `DEGRADED` | `FAULT` | 安全关键异常（运动控制失联、急停硬件故障），持续 1 个诊断周期 | 自动 |
| `FAULT` | `HEALTHY` | 人工确认（AcknowledgeFault）+ 恢复前检查通过 | 人工 |
| 任意 | `UNKNOWN` | 超过 `data_timeout` 未收到该实体健康数据 | 自动 |
| `UNKNOWN` | `HEALTHY` | 数据恢复且连续 3 个周期正常 | 自动 |

### 3.6 定级 → SM 状态映射（修复优先级对齐）

| HDS 定级结果 | 建议 SM 转换 | priority | 说明 |
|-------------|-------------|----------|------|
| `DEGRADED` | `→ DEGRADED` | **75** | 非关键模块故障 |
| `FAULT` | `→ FAULT` | **85** | 关键模块故障 |
| 安全关键 `FAULT` | `→ ACTIVE_E_STOP` | **100** | 通过 `/sm/trigger_estop` |
| `HEALTHY`（从 DEGRADED 恢复） | `→ STANDBY` | 60 | 故障恢复（需 SM 确认） |

---

## 4. ROS2 接口定义

### 4.1 消息定义（msg）

#### 保留消息（兼容原设计，新增字段用 `# [NEW]` 标注）

```
# hds_msgs/msg/DiagnosisState.msg
# 诊断状态

uint8 HEALTHY    = 0
uint8 WARNING    = 1
uint8 DEGRADED   = 2
uint8 FAULT      = 3
uint8 UNKNOWN    = 4

uint8 state                        # 当前诊断状态
string entity_id                   # 被监控实体 ID（多 SOC 下含 soc 前缀，如 "soc1/mc"）
string entity_type                 # 实体类型：module | process | hardware | sensor
string entity_name                 # 实体可读名称
string soc_id                      # [NEW] 所属 SOC ID（"soc0", "soc1" 等，空字符串=本地）
builtin_interfaces/Time timestamp  # 状态判定时间戳
string reason                      # 状态判定原因摘要
uint32 rule_id                     # 触发的规则 ID
float32 confidence                 # 诊断置信度 [0.0, 1.0]
```

```
# hds_msgs/msg/HealthReport.msg
# 各模块上报的原始健康数据

string reporter_node               # 上报节点名称
builtin_interfaces/Time timestamp  # 上报时间戳
string entity_id                   # 被监控实体 ID
string entity_type                 # 实体类型
string soc_id                      # [NEW] 来源 SOC ID（Slave 填写，本地模块留空）

HealthMetric[] metrics             # 健康指标列表
string custom_diagnosis            # JSON 格式的自定义诊断数据
```

```
# hds_msgs/msg/HealthMetric.msg
# 单个健康指标（不变）

string name                        # 指标名称
float64 value                      # 指标值
string unit                        # 单位
uint8 severity                     # 0=info, 1=warning, 2=critical
string threshold_info              # 阈值说明
```

```
# hds_msgs/msg/AlarmEvent.msg
# 告警事件

uint8 ALARM_RAISED    = 0
uint8 ALARM_CLEARED   = 1
uint8 ALARM_UPDATED   = 2

uint8 event_type                   # 告警事件类型
string alarm_id                    # 告警唯一 ID（去重用）
uint8 diagnosis_state              # 关联的诊断状态
uint8 alarm_priority               # [NEW] 告警优先级：0=low, 1=medium, 2=high, 3=critical
string entity_id                   # 关联实体 ID
string soc_id                      # [NEW] 所属 SOC ID
string summary                     # 告警摘要
string detail                      # 告警详细信息
builtin_interfaces/Time timestamp  # 事件时间戳
string[] affected_modules          # 受影响的模块列表
```

```
# hds_msgs/msg/DiagnosisChain.msg
# 诊断链路记录

string chain_id                    # 链路唯一 ID
builtin_interfaces/Time start_time # 诊断开始时间
builtin_interfaces/Time end_time   # 诊断结束时间
string entity_id                   # 被诊断实体
string soc_id                      # [NEW] 所属 SOC ID
uint8 final_state                  # 最终诊断状态
DiagnosisStep[] steps              # 诊断步骤
```

```
# hds_msgs/msg/DiagnosisStep.msg
# 单个诊断步骤（不变）

uint8 step_index
string step_name
string input_data
string rule_description
bool triggered
string output
builtin_interfaces/Time timestamp
```

```
# hds_msgs/msg/Heartbeat.msg
# HDS Master 心跳

builtin_interfaces/Time stamp
string node_name                   # 固定 "hds_master"
uint8 state                        # 当前诊断引擎状态
bool healthy                       # 诊断引擎是否健康
string status_message              # 状态描述
uint32 monitored_entities          # 当前监控实体数量（含所有 SOC）
uint32 active_alarms               # 当前活跃告警数量
uint32 connected_slaves            # [NEW] 已连接 Slave 数量
uint32 total_slaves                # [NEW] 已注册 Slave 总数
```

```
# hds_msgs/msg/ErrorCode.msg
# 错误码（不变，新增错误码在第 9 节定义）

uint16 OK                                = 0
uint16 ERR_HDS_INVALID_ENTITY            = 2001
uint16 ERR_HDS_DIAGNOSIS_TIMEOUT         = 2002
uint16 ERR_HDS_SM_REQUEST_FAILED         = 2003
uint16 ERR_HDS_RECOVERY_CHECK_FAILED     = 2004
uint16 ERR_HDS_DATA_STALE               = 2005
uint16 ERR_HDS_RULE_ENGINE_ERROR         = 2006
uint16 ERR_HDS_ENTITY_NOT_FOUND          = 2007
uint16 ERR_HDS_SLAVE_DISCONNECTED        = 2008   # [NEW]
uint16 ERR_HDS_SLAVE_AUTONOMOUS          = 2009   # [NEW]
uint16 ERR_HDS_MQTT_TIMEOUT            = 2010   # [NEW]
uint16 ERR_HDS_SLAVE_NOT_FOUND           = 2011   # [NEW]
uint16 ERR_HDS_REPLAY_IN_PROGRESS        = 2012   # [NEW]

uint16 error_code
string message
```

#### 新增消息（多 SOC 专用）

```
# hds_msgs/msg/SlaveStatus.msg
# Slave 连接状态

uint8 CONNECTED       = 0
uint8 HEARTBEAT_MISS  = 1
uint8 DISCONNECTED    = 2
uint8 AUTONOMOUS      = 3
uint8 RECONNECTING    = 4

string slave_id                    # Slave 唯一 ID（如 "soc1_motion"）
string soc_id                      # SOC ID（如 "soc1"）
uint8 status                       # 连接状态
builtin_interfaces/Time last_heartbeat  # 最后心跳时间
uint32 reported_entity_count       # 该 Slave 管辖实体数量
uint32 buffered_reports            # Slave 端缓冲中的待发报告数（由 Slave 心跳上报）
string mqtt_client_id              # Slave MQTT Client ID
```

```
# hds_msgs/msg/GlobalDiagnosisSummary.msg
# 全局诊断汇总（含各 SOC 分组信息）

builtin_interfaces/Time timestamp
uint8 overall_state                # 全局诊断状态（取最严重的）

uint32 total_entities              # 全局监控实体总数
uint32 healthy_count
uint32 warning_count
uint32 degraded_count
uint32 fault_count
uint32 unknown_count

SocHealthSummary[] soc_summaries   # 各 SOC 健康汇总
SlaveStatus[] slave_statuses       # 各 Slave 连接状态
```

```
# hds_msgs/msg/SocHealthSummary.msg
# 单个 SOC 的健康汇总

string soc_id                      # SOC ID
uint8 worst_state                  # 该 SOC 最严重的诊断状态
uint32 entity_count                # 该 SOC 实体数
uint32 healthy_count
uint32 warning_count
uint32 degraded_count
uint32 fault_count
uint32 unknown_count
bool slave_connected               # 该 SOC 的 Slave 是否连接（soc0 始终为 true）
```

### 4.2 服务定义（srv）

#### 保留服务（兼容原设计）

```
# hds_msgs/srv/QueryDiagnosis.srv
# 查询指定实体的诊断状态

# Request
string entity_id                     # 实体 ID（多 SOC 下可含 soc 前缀，如 "soc1/mc"）
---
# Response
bool found
DiagnosisState diagnosis
HealthMetric[] latest_metrics
builtin_interfaces/Time last_update
uint16 error_code
```

```
# hds_msgs/srv/ReportHealth.srv
# SOC-0 本地模块向 Master 上报健康数据（Service 方式，可选）

# Request
HealthReport report
---
# Response
bool accepted
uint16 error_code
string message
```

```
# hds_msgs/srv/RecoveryCheck.srv
# SM 调用：恢复前检查（扩展：含 Slave 状态检查）

# Request
uint8 target_state                   # 目标恢复状态
string operator_id                   # 操作者 ID
---
# Response
bool success
string message
bool allowed                         # 是否允许恢复
uint16 error_code
DiagnosisState[] failing_entities    # 仍未恢复的实体列表
SlaveStatus[] failing_slaves         # [NEW] 未就绪的 Slave 列表
```

```
# hds_msgs/srv/GetSystemHealth.srv
# 全系统健康汇总（扩展：含 SOC 维度）

# Request（空）
---
# Response
uint8 overall_state
DiagnosisState[] entity_diagnoses    # 所有实体诊断状态（含 soc_id）
uint32 healthy_count
uint32 warning_count
uint32 degraded_count
uint32 fault_count
uint32 unknown_count
SocHealthSummary[] soc_summaries     # [NEW] 各 SOC 健康汇总
SlaveStatus[] slave_statuses         # [NEW] 各 Slave 状态
```

```
# hds_msgs/srv/RegisterHealthEntity.srv
# SOC-0 本地模块注册为被监控实体

# Request
string entity_id
string entity_type
string entity_name
builtin_interfaces/Duration expected_report_interval
---
# Response
bool accepted
uint16 error_code
```

#### 新增服务（多 SOC 专用）

```
# hds_msgs/srv/GetSocHealth.srv
# 查询指定 SOC 的健康汇总

# Request
string soc_id                        # SOC ID（如 "soc1"），空字符串=全部
---
# Response
bool found
SocHealthSummary summary
DiagnosisState[] entity_diagnoses    # 该 SOC 所有实体的诊断状态
SlaveStatus slave_status             # 该 SOC 的 Slave 连接状态
uint16 error_code
```

```
# hds_msgs/srv/ListSlaves.srv
# 列出所有已注册 Slave

# Request（空）
---
# Response
SlaveStatus[] slaves
uint32 total_count
uint32 connected_count
```

### 4.3 接口汇总表

#### Topics

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/hds/health_report` | `HealthReport` | SOC-0 模块 → Master | BestEffort + Volatile + Depth 10 | 各模块自定义 | SOC-0 本地健康数据上报 |
| `/hds/diagnosis_state` | `DiagnosisState` | Master → ALL | Reliable + Volatile + Depth 1 | 事件驱动 | 全局诊断状态广播 |
| `/hds/alarm_event` | `AlarmEvent` | Master → Gateway | Reliable + Volatile + Depth 100 | 事件驱动 | 告警事件 |
| `/hds/diagnosis_chain` | `DiagnosisChain` | Master → DR | Reliable + Volatile + Depth 100 | 事件驱动 | 诊断链路记录 |
| `/hds/heartbeat` | `Heartbeat` | Master → ALL | Reliable + Volatile + Depth 1 | 1 Hz | Master 心跳 |
| `/hds/global_diagnosis_summary` | `GlobalDiagnosisSummary` | Master → ALL | Reliable + Transient Local + Depth 1 | 1 Hz | 全局诊断汇总 |
| `/hds/slave_status` | `SlaveStatus` | Master → ALL | Reliable + Volatile + Depth 10 | 事件驱动 | Slave 连接状态变更 |
| `/sm/robot_state` | `sm_msgs/RobotState` | SM → Master | — | — | Master 订阅（定级参考） |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/hds/query_diagnosis` | `QueryDiagnosis` | 任意模块 | 查询指定实体诊断状态（支持跨 SOC 查询） |
| `/hds/report_health` | `ReportHealth` | SOC-0 模块 | Service 方式上报（可选，推荐用 Topic） |
| `/hds/recovery_check` | `RecoveryCheck` | SM | 恢复前检查（含 Slave 状态） |
| `/hds/get_system_health` | `GetSystemHealth` | Gateway / 调试 | 全系统健康汇总 |
| `/hds/register_entity` | `RegisterHealthEntity` | SOC-0 模块 | 注册被监控实体（仅 SOC-0 本地） |
| `/hds/get_soc_health` | `GetSocHealth` | Gateway / 调试 | 查询指定 SOC 健康 |
| `/hds/list_slaves` | `ListSlaves` | 调试工具 | 列出已注册 Slave |

#### 调用的外部 Service

| 名称 | 类型 | 目标模块 | 说明 |
|------|------|---------|------|
| `/sm/request_transition` | `sm_msgs/RequestTransition` | SM | 请求 DEGRADED/FAULT 转换 |
| `/sm/trigger_estop` | `sm_msgs/TriggerEStop` | SM | 安全关键故障触发 E-Stop（priority=100） |

#### MQTT（跨语言 / 跨 SOC 通信）

**与 EM 交互（SOC-0 本地）：**

| Topic | 方向 | QoS | 说明 |
|-------|------|-----|------|
| `em/event/l3_suggestion` | EM → Master | 1 | L3 降级建议 |
| `em/event/l4_suggestion` | EM → Master | 1 | L4 紧急停止建议 |
| `hds/command/recovery_ack` | Master → EM | 1 | 恢复批准/拒绝 |

**与 HDS Slave 交互（跨 SOC）：**

| Topic | 方向 | QoS | Retain | 说明 |
|-------|------|-----|--------|------|
| `hds/slave/{slave_id}/heartbeat` | S → M | 0 | 否 | Slave 心跳（1Hz） |
| `hds/master/heartbeat` | M → ALL | 0 | 否 | Master 心跳（1Hz） |
| `hds/slave/{slave_id}/health_batch` | S → M | 0 | 否 | 批量健康数据上报（200ms 窗口） |
| `hds/slave/{slave_id}/critical` | S → M | 1 | 否 | 关键异常立即上报 |
| `hds/slave/{slave_id}/autonomous` | S → M | 1 | 否 | Slave 自治模式通告 |
| `hds/slave/{slave_id}/status` | S → M | 1 | **是** | Slave 注册状态（Will Message 用于断联检测） |
| `hds/master/global_state` | M → ALL | 0 | 否 | 全局诊断状态推送 |
| `hds/slave/{slave_id}/command` | M → S | 1 | 否 | 配置/查询命令下发 |
| `hds/slave/{slave_id}/command_result` | S → M | 1 | 否 | 命令执行结果 |
| `hds/slave/{slave_id}/replay` | S → M | 1 | 否 | 缓冲数据回放（含 begin/batch/end） |

Master 使用通配符订阅 `hds/slave/+/#` 实现自动扩展（新增 Slave 无需修改 Master 配置）。

---

## 5. MQTT 通信设计（Master ↔ Slave）

### 5.1 设计总览

HDS Master 与 Slave 之间的通信**复用系统现有 MQTT Broker**（由 EM 管理，部署在 SOC-0），独立于 ROS2 DDS，不依赖跨 SOC 的 DDS 发现机制。

**选择 MQTT 的理由**：
- **基础设施复用**：系统已有 MQTT Broker 用于 EM ↔ HDS 通信，无需额外部署
- **即时断联检测**：Will Message 机制自动通告 Slave 断联，延迟 < 1s
- **灵活扩展**：Master 通配符订阅 `hds/slave/+/#`，新增 Slave 零配置
- **QoS 分级**：QoS 0 用于批量数据（低延迟、允许丢失），QoS 1 用于关键上报（至少一次送达）
- **调试友好**：JSON 负载可直接 `mosquitto_sub` 观察，降低联调门槛
- **代码精简**：无需自定义帧格式、序列化库、连接管理，比自研 TCP Bridge 减少约 60% 代码

### 5.2 通信拓扑

```
MQTT Broker (SOC-0, port 1883, EM 管理)
  ├── HDS Master — 订阅 hds/slave/+/#
  ├── HDS Slave-1 (soc1_motion) — 发布 hds/slave/soc1_motion/...
  ├── HDS Slave-2 (soc2_perception) — 发布 hds/slave/soc2_perception/...
  └── ...（最多 MAX_SLAVES 个 Slave 客户端）
```

- Master 和每个 Slave 各自维护独立 MQTT 连接
- Master 使用通配符订阅 `hds/slave/+/#`，自动发现新 Slave
- Slave 订阅 `hds/master/#` + `hds/slave/{self_id}/command`
- Broker 断开后 Slave 自动重连（指数退避：1s → 2s → 4s → 8s → 最大 30s）

### 5.3 Topic 命名与 QoS 映射

| Topic Pattern | 方向 | QoS | Retain | 说明 |
|---------------|------|-----|--------|------|
| `hds/slave/{slave_id}/status` | S → M | 1 | **是** | Slave 注册信息（Retained + Will Message） |
| `hds/slave/{slave_id}/heartbeat` | S → M | 0 | 否 | Slave 心跳（1Hz） |
| `hds/slave/{slave_id}/health_batch` | S → M | 0 | 否 | 批量健康数据（200ms 窗口聚合） |
| `hds/slave/{slave_id}/critical` | S → M | 1 | 否 | 关键异常立即上报（E-Stop 通道） |
| `hds/slave/{slave_id}/autonomous` | S → M | 1 | 否 | 自治模式通告 |
| `hds/slave/{slave_id}/replay` | S → M | 1 | 否 | 缓冲数据回放（begin/batch/end） |
| `hds/slave/{slave_id}/command_result` | S → M | 1 | 否 | 命令执行结果 |
| `hds/master/heartbeat` | M → ALL | 0 | 否 | Master 心跳（1Hz） |
| `hds/master/global_state` | M → ALL | 0 | 否 | 全局诊断状态推送 |
| `hds/slave/{slave_id}/command` | M → S | 1 | 否 | 配置/查询命令下发 |

**QoS 选择原则**：
- **QoS 0**（At most once）：批量健康数据、心跳 — 允许偶尔丢失，下一个周期会覆盖
- **QoS 1**（At least once）：关键上报、注册状态、命令 — 必须送达，允许重复（接收端幂等处理）

### 5.4 JSON 负载格式

所有 MQTT 消息使用 **JSON** 序列化（取代 Protobuf），便于调试和跨语言解析。

#### 5.4.1 Slave 注册（status — Retained Message）

Slave 连接 Broker 时发布 Retained Message 到 `hds/slave/{slave_id}/status`：

```json
{
  "slave_id": "soc1_motion",
  "soc_id": "soc1",
  "protocol_version": 1,
  "entities": [
    {
      "entity_id": "mc",
      "entity_type": "module",
      "entity_name": "Motion Control",
      "expected_report_interval_ms": 100,
      "is_critical": true
    },
    {
      "entity_id": "hal_ethercat",
      "entity_type": "hardware",
      "entity_name": "EtherCAT HAL",
      "expected_report_interval_ms": 50,
      "is_critical": true
    }
  ],
  "software_version": "1.0.0",
  "status": "online",
  "timestamp_ns": 1716825600000000000
}
```

**Will Message 配置**：Slave 连接时设置 Will Topic = `hds/slave/{slave_id}/status`，Will Payload = `{"slave_id":"...","status":"offline","timestamp_ns":...}`，Will QoS = 1，Will Retain = true。Broker 在 Slave 异常断联时自动发布此消息，Master 立即感知。

Master 收到 `status` 消息后的处理：
- `"status": "online"` + 新 slave_id → 注册新 Slave，响应 `hds/slave/{slave_id}/command` 下发配置
- `"status": "online"` + 已知 slave_id → 重连处理，进入 RECONNECTING 流程
- `"status": "offline"` → Will Message 触发，Slave 断联处理

#### 5.4.2 Slave 心跳

```json
{
  "slave_id": "soc1_motion",
  "timestamp_ns": 1716825600000000000,
  "diagnosis_engine_healthy": true,
  "buffered_reports": 0,
  "mode": "NORMAL",
  "local_entity_count": 5,
  "local_fault_count": 0
}
```

`mode` 枚举值：`"NORMAL"`, `"AUTONOMOUS"`, `"RECONNECTING"`

#### 5.4.3 Master 心跳

```json
{
  "timestamp_ns": 1716825600000000000,
  "global_diagnosis_state": 0,
  "master_healthy": true,
  "sm_robot_state": 3
}
```

#### 5.4.4 批量健康数据

```json
{
  "slave_id": "soc1_motion",
  "batch_seq": 42,
  "is_replay": false,
  "timestamp_ns": 1716825600000000000,
  "reports": [
    {
      "reporter_node": "mc_node",
      "timestamp_ns": 1716825599800000000,
      "entity_id": "mc",
      "entity_type": "module",
      "metrics": [
        {"name": "cpu_percent", "value": 45.2, "unit": "%", "severity": 0, "threshold_info": "<80%"},
        {"name": "cycle_overrun_count", "value": 0, "unit": "count", "severity": 0, "threshold_info": "<5"}
      ],
      "custom_diagnosis": ""
    }
  ]
}
```

#### 5.4.5 关键异常上报

```json
{
  "slave_id": "soc1_motion",
  "entity_id": "hal_ethercat",
  "severity": 3,
  "reason": "EtherCAT bus error: working counter mismatch",
  "timestamp_ns": 1716825600000000000,
  "autonomous_estop_triggered": false,
  "triggering_metrics": [
    {"name": "bus_error_count", "value": 5, "unit": "count", "severity": 2, "threshold_info": ">=3"}
  ]
}
```

#### 5.4.6 自治模式通告

```json
{
  "slave_id": "soc1_motion",
  "timestamp_ns": 1716825600000000000,
  "reason": "master_heartbeat_timeout",
  "local_estop_triggered": true,
  "faulted_entities": ["hal_ethercat", "mc"]
}
```

#### 5.4.7 全局状态推送

```json
{
  "timestamp_ns": 1716825600000000000,
  "global_diagnosis_state": 0,
  "sm_robot_state": 3,
  "entity_states": [
    {"entity_id": "soc1/mc", "soc_id": "soc1", "diagnosis_state": 0},
    {"entity_id": "soc1/hal_ethercat", "soc_id": "soc1", "diagnosis_state": 0}
  ]
}
```

#### 5.4.8 命令下发与结果

Master → Slave 命令：
```json
{
  "command_id": 1001,
  "type": "QUERY_LOCAL_HEALTH",
  "payload": {}
}
```

`type` 枚举值：`"QUERY_LOCAL_HEALTH"`, `"UPDATE_CRITICAL_LIST"`, `"UPDATE_RULES"`, `"FORCE_REPORT"`

Slave → Master 命令结果：
```json
{
  "command_id": 1001,
  "success": true,
  "error_message": "",
  "payload": {}
}
```

#### 5.4.9 数据回放

回放开始（Slave 重连后）：
```json
{
  "type": "begin",
  "slave_id": "soc1_motion",
  "disconnect_timestamp_ns": 1716825500000000000,
  "reconnect_timestamp_ns": 1716825600000000000,
  "total_buffered_reports": 150
}
```

回放批次（复用 health_batch 格式，`is_replay: true`）通过 `hds/slave/{slave_id}/replay` 发送。

回放结束：
```json
{
  "type": "end",
  "slave_id": "soc1_motion",
  "total_replayed": 148,
  "total_dropped": 2
}
```

### 5.5 注册与断联检测机制



<whiteboard type="blank"></whiteboard>



**关键设计点**：
- **Retained Message** 保证 Master 重启后能立即获取所有 Slave 的最后已知状态
- **Will Message** 由 Broker 负责发布，无需 Slave 主动通告断联，延迟仅取决于 MQTT keepalive（建议 2s，断联检测 ≤ 3s）
- **CleanSession=false** 确保 Slave 重连后接收到断联期间的 QoS 1 命令消息

### 5.6 上报策略

```
Slave 上报双通道：

1. Batch Channel（普通数据，200ms 聚合窗口）
   ├─ 同一 entity_id 在窗口内只保留最新值（幂等合并）
   ├─ 超过 MAX_BATCH_SIZE (100) 条时提前发送
   ├─ MQTT QoS 0：允许偶发丢失，下一窗口覆盖
   └─ Topic: hds/slave/{slave_id}/health_batch

2. Critical Channel（关键异常，立即发送）
   ├─ LocalDiagEngine 判定 severity ≥ FAULT 时触发
   ├─ 不等批次窗口，立即发布到 hds/slave/{slave_id}/critical
   ├─ MQTT QoS 1：确保至少一次送达
   ├─ Master 端幂等处理（基于 entity_id + timestamp_ns 去重）
   └─ 3 × heartbeat 周期内未收到 Master 心跳 → 进入 Autonomous 模式
```

---

## 6. 内部设计

### 6.1 节点架构



<whiteboard type="blank"></whiteboard>



### 6.2 Callback Group 隔离

| Callback Group | 类型 | 包含的回调 | 理由 |
|---------------|------|-----------|------|
| `default_cbg` | MutuallyExclusive | 诊断引擎定时器、状态发布、大部分 Service | 主诊断循环，避免并发竞争 |
| `mqtt_bridge_cbg` | MutuallyExclusive | MqttBridge 回调、Slave 心跳检查 | MQTT I/O 与诊断引擎解耦 |
| `estop_cbg` | MutuallyExclusive | E-Stop 相关的 Critical Report 处理 | **独立 Callback Group**，E-Stop 路径不可被阻塞 |
| `recovery_cbg` | MutuallyExclusive | RecoveryCheck Service | 避免恢复检查阻塞主循环 |

### 6.3 关键流程

#### 6.3.1 正常诊断流程（含远端 Slave 数据）

```
SOC-1 的 MC 模块
  → 上报 /hds/health_report（SOC-1 本地 ROS2 域）
    → HDS Slave-1 LocalCollector 收集
      → MqttBridge 批量打包 (200ms 窗口)
        → MQTT QoS 0 → hds/slave/soc1_motion/health_batch → Broker → Master MqttBridge
          → SlaveDataAggregator 解析 JSON
            → entity_id "mc" → "soc1/mc"
              → 注入 GlobalDiagEngine
                → RuleEvaluator 匹配规则
                  → 跨 SOC 关联规则检查
                    → SeverityMapper 定级
                      → StateTransitionArbiter 决策
                        → 需请求 SM？
                          → 是：/sm/request_transition (priority=75|85)
                          → 否：仅发布 /hds/diagnosis_state
```

#### 6.3.2 安全关键故障 E-Stop 触发流程

```
SOC-1 的 HAL_EtherCAT 总线错误
  → Slave-1 LocalDiagEngine 判定 severity=FAULT (rule: ethercat_bus_fault)
    → MqttBridge 立即发布 hds/slave/soc1_motion/critical（QoS 1，不等批次窗口）
      → Broker → Master MqttBridge 收到
        → estop_cbg 回调处理（独立 Callback Group，不可阻塞）
          → GlobalDiagEngine 评估
            → 规则 "ethercat_bus_fault" 标记 trigger_estop=true
              → StateTransitionArbiter 识别 E-Stop 规则
                → 直接调用 /sm/trigger_estop (priority=100)
                  → SM 进入 ACTIVE_E_STOP
                    → AlarmManager 生成 critical 告警
                      → /hds/alarm_event → Gateway → 云端
```

**时序要求**：CRITICAL_REPORT 经 Broker 到达 Master → `/sm/trigger_estop` 发出，端到端延迟 < 50ms（MQTT Broker 本地转发延迟 < 1ms）。

#### 6.3.3 Slave 断联处理流程

```
两种断联检测路径（取先触发者）：

路径 A — Will Message（即时）：
  Slave 网络中断 → Broker 检测 keepalive 超时
    → 自动发布 hds/slave/{id}/status (status="offline", Retained)
      → Master MqttBridge 收到 → Slave 状态 → DISCONNECTED

路径 B — 心跳超时（兜底）：
  Master SlaveRegistry 定时检查（1Hz）
    → Slave 心跳超时（连续 3 次未收到）→ Slave 状态 → DISCONNECTED

断联后处理：
  Slave 状态 → DISCONNECTED
    → 发布 /hds/slave_status (DISCONNECTED)
      → 遍历 Slave 管辖实体
        → 关键实体（mc, hal_ethercat 等）→ 定级为 FAULT
          → StateTransitionArbiter → /sm/request_transition (FAULT, priority=85)
        → 非关键实体 → 定级为 UNKNOWN
          → 仅发布 /hds/diagnosis_state
      → 等待 Slave 重连
        → 收到 hds/slave/{id}/status (status="online") → 状态 → RECONNECTING
          → 接收 hds/slave/{id}/replay (begin → batch × N → end)
            → 回放数据注入 GlobalDiagEngine 重新评估
              → 状态 → CONNECTED
```

#### 6.3.4 恢复前检查流程（多 SOC 扩展）

```
SM 收到 AcknowledgeFault 请求
  → SM 调用 /hds/recovery_check (target_state=STANDBY)
    → RecoveryCheckHandler（recovery_cbg，独立 Callback Group）
      → Step 1：检查 SOC-0 本地诊断实体
        → 关键实体中任一为 FAULT/UNKNOWN？→ 拒绝
      → Step 2：检查所有 Slave 连接状态
        → 任一 Slave 为 DISCONNECTED/AUTONOMOUS？→ 拒绝
        → 任一 Slave 为 RECONNECTING（回放中）？→ 拒绝
      → Step 3：向每个 CONNECTED Slave 发布 hds/slave/{id}/command (QUERY_LOCAL_HEALTH)
        → 等待 hds/slave/{id}/command_result 响应（超时 1s，总体 2s 硬限制）
          → 任一 Slave 报告本地 FAULT 实体？→ 拒绝
      → 全部通过 → 返回 allowed=true
```

**重要**：恢复前检查**不检查 EM 进程状态**（EM 进程由 SM 直接调用 EM 的 `GetProcessStatus` 检查）。

### 6.4 跨 SOC 关联规则示例

```yaml
# hds_master_rules.yaml 中的跨 SOC 关联规则

cross_soc_rules:
  - rule_id: 5001
    name: "motion_soc_total_failure"
    description: "运动 SOC 整体失联且 SM 处于 ACTIVE 状态 → E-Stop"
    condition:
      type: "cross_soc"
      expression: >
        slave_status("soc1") == DISCONNECTED
        AND sm_robot_state IN [MANUAL, AUTO, MOTION_STREAM]
    severity: "FAULT"
    trigger_estop: true

  - rule_id: 5002
    name: "ethercat_fault_with_motion_active"
    description: "EtherCAT 总线故障 + 运动控制正在执行 → E-Stop"
    condition:
      type: "cross_soc"
      expression: >
        entity_state("soc1/hal_ethercat") == FAULT
        AND entity_state("soc0/mc") != HEALTHY
    severity: "FAULT"
    trigger_estop: true

  - rule_id: 5003
    name: "perception_soc_degraded_during_auto"
    description: "感知 SOC 降级 + 自动模式 → 请求降级"
    condition:
      type: "cross_soc"
      expression: >
        soc_worst_state("soc2") >= DEGRADED
        AND sm_robot_state == AUTO
    severity: "DEGRADED"
    trigger_estop: false
```

---

## 7. 与其他模块的交互

### 7.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|------|------|------|------|
| SM | Master → SM | `/sm/request_transition` (Service) | 请求降级/故障转换（DEGRADED: priority=75, FAULT: priority=85） |
| SM | Master → SM | `/sm/trigger_estop` (Service) | 安全关键故障触发 E-Stop（priority=100） |
| SM | SM → Master | `/hds/recovery_check` (Service) | 恢复前检查 |
| SM | SM → Master | `/sm/robot_state` (Topic) | Master 订阅，定级决策参考 |
| EM | EM → Master | `/hds/health_report` (Topic) | SOC-0 EM 上报进程状态 |
| EM | EM → Master | MQTT `em/event/l3_suggestion` | L3 降级建议 |
| EM | EM → Master | MQTT `em/event/l4_suggestion` | L4 紧急停止建议 |
| EM | Master → EM | MQTT `hds/command/recovery_ack` | 恢复批准/拒绝 |
| Gateway | Master → GW | `/hds/alarm_event` (Topic) | 告警上报 |
| Gateway | GW → Master | `/hds/get_system_health` (Service) | 健康汇总查询 |
| Gateway | GW → Master | `/hds/get_soc_health` (Service) | SOC 维度查询 |
| DR | Master → DR | `/hds/diagnosis_chain` (Topic) | 诊断链路记录 |
| HDS Slave | Slave → Master | MQTT `hds/slave/{id}/*` | 注册、心跳、健康数据批量/即时上报、自治通告 |
| HDS Slave | Master → Slave | MQTT `hds/master/*`, `hds/slave/{id}/command` | 心跳、全局状态推送、命令下发 |
| SOC-0 各模块 | 模块 → Master | `/hds/health_report` (Topic) | 本地模块直接上报 |
| SOC-0 各模块 | 模块 → Master | `/hds/register_entity` (Service) | 注册被监控实体 |

### 7.2 关键交互时序

#### 多 SOC 故障检测与定级



<whiteboard type="blank"></whiteboard>



---

## 8. 关键参数与配置

```yaml
# hds_master/config/hds_master_params.yaml

hds_master:
  ros__parameters:
    # === 诊断引擎 ===
    diagnosis_rate_hz: 1.0
    data_timeout_multiplier: 3.0

    # 状态转换去抖周期数
    warning_to_degraded_cycles: 2
    degraded_to_fault_cycles: 1
    recovery_confirmation_cycles: 5

    # 趋势分析
    trend_window_sec: 60.0

    # === SM 请求 ===
    sm_request_retry_count: 3
    sm_request_retry_backoff_base_sec: 5.0   # 指数退避基数：5s → 10s → 20s
    sm_request_retry_backoff_max_sec: 60.0
    sm_priority_degraded: 75
    sm_priority_fault: 85
    sm_priority_estop: 100

    # === 告警管理 ===
    alarm_suppression_enabled: true
    alarm_safety_critical_bypass_suppression: true  # 安全关键告警不可抑制
    alarm_escalation_intervals_sec: [300, 900, 1800]
    max_alarms_retained: 1000

    # === 诊断日志 ===
    diagnosis_history_size: 500

    # === SOC-0 本地关键实体 ===
    local_critical_entities:
      - "sm"
      - "em"
      - "gateway"

    # === 规则配置 ===
    local_rule_config_path: "config/hds_master_rules.yaml"
    cross_soc_rule_config_path: "config/hds_cross_soc_rules.yaml"

    # === 多 SOC / MQTT Bridge ===
    multi_soc:
      enabled: true                      # false 时退化为单 SOC 模式
      mqtt_broker_address: "127.0.0.1"   # MQTT Broker 地址（SOC-0 本地）
      mqtt_broker_port: 1883             # MQTT Broker 端口
      mqtt_client_id: "hds_master"
      mqtt_keepalive_sec: 2              # MQTT keepalive（断联检测 ≤ 3s）
      max_slaves: 8
      slave_heartbeat_timeout_sec: 3.0   # Slave 心跳超时（3 × 心跳间隔）
      slave_heartbeat_miss_threshold: 3  # 连续丢失 N 次标记 DISCONNECTED
      slave_batch_interval_ms: 200       # 下发给 Slave 的批量上报周期
      reconnect_replay_timeout_sec: 30.0 # 回放数据的总超时时间

    # === 高可用 ===
    high_availability:
      enabled: false                     # true 时启用 Master 热备
      standby_port: 7891
      state_sync_interval_ms: 1000       # 主备状态同步周期
```

---

## 9. 错误码定义

| 错误码 | 常量名 | 说明 |
|--------|--------|------|
| 0 | `OK` | 成功 |
| 2001 | `ERR_HDS_INVALID_ENTITY` | 未知监控实体 |
| 2002 | `ERR_HDS_DIAGNOSIS_TIMEOUT` | 诊断处理超时 |
| 2003 | `ERR_HDS_SM_REQUEST_FAILED` | 向 SM 请求状态转换失败 |
| 2004 | `ERR_HDS_RECOVERY_CHECK_FAILED` | 恢复前检查未通过 |
| 2005 | `ERR_HDS_DATA_STALE` | 数据过旧，诊断不可靠 |
| 2006 | `ERR_HDS_RULE_ENGINE_ERROR` | 规则引擎内部错误 |
| 2007 | `ERR_HDS_ENTITY_NOT_FOUND` | 查询的实体不存在 |
| 2008 | `ERR_HDS_SLAVE_DISCONNECTED` | Slave 已断联 |
| 2009 | `ERR_HDS_SLAVE_AUTONOMOUS` | Slave 处于自治模式 |
| 2010 | `ERR_HDS_MQTT_TIMEOUT` | MQTT 通信超时 |
| 2011 | `ERR_HDS_SLAVE_NOT_FOUND` | 查询的 Slave 不存在 |
| 2012 | `ERR_HDS_REPLAY_IN_PROGRESS` | Slave 正在回放缓冲数据 |
| 2013 | `ERR_HDS_ESTOP_TRIGGER_FAILED` | E-Stop 触发失败 |
| 2014 | `ERR_HDS_CROSS_SOC_RULE_ERROR` | 跨 SOC 规则评估错误 |
| 2015 | `ERR_HDS_SLAVE_REGISTER_FULL` | Slave 注册已满（超过 max_slaves） |

---

## 10. 安全约束

### 10.1 全局定级权威

- HDS Master 是系统中**唯一有权决定故障等级**的模块
- Slave 的 LocalDiagEngine 只做预过滤，不产出最终定级
- EM 的 L3/L4 建议仅作为输入参考，最终定级由 Master 决策

### 10.2 E-Stop 触发路径

- 安全关键规则（`trigger_estop: true`）触发时，Master 调用 `/sm/trigger_estop`（priority=100）
- E-Stop 路径使用**独立 Callback Group**（`estop_cbg`），不可被诊断引擎或 MQTT I/O 阻塞
- CRITICAL_REPORT 到达 Master 到 E-Stop 请求发出，端到端延迟目标 < 50ms

### 10.3 Slave 断联 = 不安全

- Slave 管辖的关键实体在 Slave 断联后，**默认定级为 FAULT**
- Slave DISCONNECTED 且管辖运动相关实体时，触发 E-Stop
- Slave 断联期间 Master 不接受涉及该 SOC 的恢复请求

### 10.4 恢复前检查

- `/hds/recovery_check` 检查范围：HDS 自身管辖的诊断数据 + 所有 Slave 连接状态
- **不检查 EM 进程状态**（EM 进程由 SM 直接调用 EM 的 `GetProcessStatus` 检查）
- 任一 Slave 为 DISCONNECTED/AUTONOMOUS/RECONNECTING → 拒绝恢复
- 恢复前检查必须在 **2 秒内**完成，超时视为检查失败

### 10.5 告警抑制安全

- 安全关键告警（`alarm_priority=critical` 且涉及运动安全实体）**不受抑制规则影响**
- 被抑制的告警仍记录到 `/hds/diagnosis_chain`，不可丢失

### 10.6 Master 自身健康

- Master 定期自检：规则引擎、MQTT 连接状态、SM 连接、数据缓冲区
- Master 自身故障时通过 `/hds/heartbeat` 的 `healthy=false` 广播
- 高可用模式下 Standby 检测 Active 故障后自动提升
- SM 检测到 Master 心跳超时后进入保守默认行为（拒绝新运动指令）

### 10.7 MQTT 通信安全

- MQTT Broker 仅监听内部网络接口（非公网），由 EM 管理 Broker 进程
- Slave 注册通过 Retained Message 携带 soc_id + entity 清单，Master 校验后才写入 SlaveRegistry
- Master 使用 MQTT topic ACL 限制：只订阅 `hds/slave/+/#` 和 `em/event/#`，不暴露其他 topic
- 未注册 Slave 的 heartbeat/health_batch 消息：Master 解析 slave_id 后若未在 SlaveRegistry 中找到则丢弃
- JSON 负载大小限制：单条消息最大 64KB，超过则丢弃并记录告警

---

## 11. 包结构

```
hds_msgs/
    msg/
        DiagnosisState.msg
        HealthReport.msg
        HealthMetric.msg
        AlarmEvent.msg
        DiagnosisChain.msg
        DiagnosisStep.msg
        Heartbeat.msg
        ErrorCode.msg
        SlaveStatus.msg              # [NEW]
        GlobalDiagnosisSummary.msg   # [NEW]
        SocHealthSummary.msg         # [NEW]
    srv/
        QueryDiagnosis.srv
        ReportHealth.srv
        RecoveryCheck.srv
        GetSystemHealth.srv
        RegisterHealthEntity.srv
        GetSocHealth.srv             # [NEW]
        ListSlaves.srv               # [NEW]
    CMakeLists.txt
    package.xml

hds_master/
    include/hds_master/
        hds_master_node.hpp          # 主节点类
        health_data_collector.hpp    # SOC-0 本地数据汇聚
        entity_registry.hpp          # 全局实体注册表
        slave_registry.hpp           # [NEW] Slave 连接管理
        mqtt_bridge.hpp              # [NEW] MQTT Bridge 通信层
        slave_data_aggregator.hpp    # [NEW] Slave 数据聚合器
        global_diag_engine.hpp       # 全局诊断引擎（含跨 SOC 规则）
        rule_evaluator.hpp           # 规则评估器
        trend_analyzer.hpp           # 趋势分析器
        severity_mapper.hpp          # 严重度映射器
        state_transition_arbiter.hpp # 状态转换仲裁器
        alarm_manager.hpp            # 告警管理器
        recovery_check_handler.hpp   # 恢复前检查处理器
        mqtt_subscriber.hpp          # MQTT 订阅（EM L3/L4）
    src/
        hds_master_node.cpp
        health_data_collector.cpp
        entity_registry.cpp
        slave_registry.cpp
        mqtt_bridge.cpp
        slave_data_aggregator.cpp
        global_diag_engine.cpp
        rule_evaluator.cpp
        trend_analyzer.cpp
        severity_mapper.cpp
        state_transition_arbiter.cpp
        alarm_manager.cpp
        recovery_check_handler.cpp
        mqtt_subscriber.cpp
    test/
        test_rule_evaluator.cpp
        test_global_diag_engine.cpp
        test_alarm_manager.cpp
        test_recovery_check.cpp
        test_slave_registry.cpp
        test_mqtt_bridge.cpp
        test_cross_soc_rules.cpp
        test_integration.cpp
    config/
        hds_master_params.yaml
        hds_master_rules.yaml
        hds_cross_soc_rules.yaml
    launch/
        hds_master.launch.py
    CMakeLists.txt
    package.xml
```
