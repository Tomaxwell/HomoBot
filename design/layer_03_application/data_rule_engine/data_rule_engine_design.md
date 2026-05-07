# Data Rule Engine 模块设计

## 1. 模块概述与定位

**模块名称**：Data Rule Engine（端侧数据规则引擎）

**定位**：Data Rule Engine 是端侧软件系统中**数据采集策略的解析与执行中枢**，位于应用层。它接收云端下发的采集规则，持续订阅系统 Topic 并评估触发条件，在条件满足时自动触发 DR（Data Recorder）进行数据采集，或触发 Data Uploader 进行数据上传。它是实现"主动采集-评估-上传"数据飞轮闭环的核心模块。

**核心职责**：

1. **规则解析**：加载并验证云端下发的规则 YAML，构建可执行的规则对象
2. **Topic 监控**：订阅规则中指定的 Topic，实时评估触发条件
3. **状态条件监控**：监听 SM 状态、HDS 诊断结果等系统状态变化
4. **规则触发**：条件满足时向 DR 发送采集指令，向 Data Uploader 发送上传指令
5. **冲突仲裁**：多条规则同时触发时按优先级排序，避免资源冲突
6. **规则热更新**：支持规则动态加载/卸载/更新，无需重启节点
7. **执行追踪**：记录每条规则的触发历史，支持数据溯源

**与相邻模块的边界**：

| 边界 | Data Rule Engine 负责 | 对方负责 |
|------|----------------------|---------|
| Data Rule Engine ↔ Gateway | 接收云端下发的规则 YAML | 云端连接、规则下发通道管理 |
| Data Rule Engine ↔ DR | 发送采集触发指令（时长、标签、优先级） | 实际数据录制、存储、质量评估 |
| Data Rule Engine ↔ Data Uploader | 发送上传触发指令（资产 ID、优先级） | 实际上传调度、断点续传 |
| Data Rule Engine ↔ SM | 读取当前机器人状态用于条件判断 | 状态机管理、状态转换决策 |
| Data Rule Engine ↔ HDS | 读取诊断结果用于条件判断 | 故障诊断、定级 |
| Data Rule Engine ↔ Setting | 读取规则引擎配置参数 | 参数持久化与验证 |

---

## 2. 职责边界

**Data Rule Engine 不做的事情**（红线）：

- **不做数据采集** — 只负责触发 DR，实际录制由 DR 执行
- **不做数据存储** — 只负责触发上传，实际存储由 DR / Data Uploader 管理
- **不做数据质量评估** — 质量评分是 Data Quality Filter 的职责
- **不做控制指令生成** — 不直接控制机器人运动
- **不做故障诊断** — 只读取 HDS 的诊断结果，不做独立诊断
- **不做规则制定** — 规则由云端制定，端侧只负责解析执行

---

## 3. 状态机设计

### 3.1 规则状态枚举

| 状态 | 值 | 说明 |
|------|-----|------|
| `RULE_ACTIVE` | 0 | 规则已加载，正在监控 |
| `RULE_TRIGGERED` | 1 | 规则已触发，等待执行完成 |
| `RULE_COOLING` | 2 | 触发后冷却期，防止过度触发 |
| `RULE_DISABLED` | 3 | 规则被禁用 |
| `RULE_ERROR` | 4 | 规则解析或执行出错 |

### 3.2 模块运行状态机

| 状态 | 值 | 说明 |
|------|-----|------|
| `DRE_STATE_STANDBY` | 0 | 待机，等待规则加载 |
| `DRE_STATE_ACTIVE` | 1 | 活跃，规则监控中 |
| `DRE_STATE_RULE_UPDATING` | 2 | 正在热更新规则 |
| `DRE_STATE_FAULT` | 3 | 模块故障（规则解析失败/订阅异常） |

### 3.3 状态转换图

```
                     rules_loaded
              ┌────────────────────────┐
              │                        ▼
        ┌─────┴────────┐         ┌──────────────┐
   ┌──► │ DRE_STANDBY  │────────►│  DRE_ACTIVE  │
   │    │      0       │         │      1       │
   │    └──────────────┘         └──────┬───────┘
   │           ▲                        │
   │           │                        │ rule_update_request
   │    rules_cleared                   ▼
   │           │                 ┌──────────────┐
   │           │                 │ DRE_RULE     │
   │           │                 │  UPDATING    │
   │           │                 │      2       │
   │           │                 └──────┬───────┘
   │           │                        │ update_complete
   │           │                        │
   │           │                        ▼
   │    ┌──────┘                 ┌──────────────┐
   │    ▼                        │   DRE_FAULT  │
   │  ┌──────────────┐           │      3       │
   └──┤  DRE_FAULT   │◄──────────┘              │
      │      3       │                          │
      └──────────────┘           fault_occurred │
             ▲                                  │
             │ recover                           │
             └──────────────────────────────────┘
```

---

## 4. ROS2 接口定义

### 4.1 消息定义（data_rule_msgs）

```
# data_rule_msgs/msg/RuleDefinition.msg
# 规则定义（触发条件 + 动作）

string rule_id                    # 规则唯一ID
string name                       # 规则名称
string description                # 规则描述
uint8 priority                    # 优先级（0-255，越小越高）
string trigger_type               # "topic" / "state" / "time" / "random"
string trigger_condition          # 条件表达式字符串
string action_type                # "dr_record" / "upload" / "both"
uint8 action_duration_before_sec  # 触发前录制时长（秒）
uint8 action_duration_after_sec   # 触发后录制时长（秒）
string[] tags                     # 数据标签
bool auto_upload                  # 是否自动上传
uint8 cooldown_sec                # 冷却时间（秒）
bool enabled                      # 是否启用
```

```
# data_rule_msgs/msg/RuleTriggerEvent.msg
# 规则触发事件

string rule_id                    # 触发的规则ID
string rule_name                  # 规则名称
builtin_interfaces/Time triggered_at
string trigger_reason             # 触发原因描述
string[] matched_topics           # 匹配的Topic列表
string matched_condition          # 匹配的具体条件
```

```
# data_rule_msgs/msg/RuleExecutionLog.msg
# 规则执行日志

string log_id                     # 日志ID
string rule_id                    # 规则ID
builtin_interfaces/Time triggered_at
builtin_interfaces/Time completed_at
string action_result              # "success" / "failed" / "cancelled"
string dr_session_id              # 关联的DR录制会话ID
string error_message              # 错误信息
```

```
# data_rule_msgs/msg/Heartbeat.msg
# 模块心跳

builtin_interfaces/Time stamp
uint8 state                       # 模块运行状态
uint32 error_code                 # 当前错误码（0 = OK）
uint32 active_rule_count          # 活跃规则数量
uint32 triggered_today            # 今日触发次数
```

### 4.2 服务定义（data_rule_msgs）

```
# data_rule_msgs/srv/DeployRules.srv
# 云端下发规则（支持热更新）

string rules_yaml                 # 规则YAML字符串（Base64编码）
bool replace_all                  # 是否替换全部规则（false = 增量更新）
---
bool success
string message
string[] deployed_rule_ids
string[] failed_rule_ids
```

```
# data_rule_msgs/srv/GetActiveRules.srv
# 查询当前生效规则

---
bool success
data_rule_msgs/RuleDefinition[] rules
```

```
# data_rule_msgs/srv/EnableRule.srv
# 启用/禁用指定规则

string rule_id
bool enabled
---
bool success
string message
```

### 4.3 Topic 汇总

| Topic | 类型 | 流向 | QoS | 频率 | 说明 |
|-------|------|------|-----|------|------|
| `/data_rule_engine/rule_trigger_event` | `RuleTriggerEvent` | DRE → ALL | Reliable + Volatile | 事件驱动 | 规则触发事件广播 |
| `/data_rule_engine/rule_execution_log` | `RuleExecutionLog` | DRE → ALL | Reliable + Volatile | 事件驱动 | 规则执行日志 |
| `/data_rule_engine/heartbeat` | `Heartbeat` | DRE → EM/HDS | Reliable + Volatile | 1Hz | 模块心跳 |
| `/sm/robot_state` | `RobotState` | SM → DRE | Reliable + Volatile | 事件驱动 | 状态条件判断 |
| `/hds/diagnosis_event` | `DiagnosisEvent` | HDS → DRE | Reliable + Volatile | 事件驱动 | 诊断条件判断 |
| `/agent/decision_quality` | `DecisionQuality` | Agent → DRE | Reliable + Volatile | 事件驱动 | 置信度条件判断 |

### 4.4 Service 汇总

| Service | 类型 | 调用方 | 说明 |
|---------|------|--------|------|
| `/data_rule_engine/deploy_rules` | `DeployRules` | Gateway | 云端下发/更新规则 |
| `/data_rule_engine/get_active_rules` | `GetActiveRules` | TE / Gateway | 查询当前规则 |
| `/data_rule_engine/enable_rule` | `EnableRule` | TE / Gateway | 启用/禁用规则 |

### 4.5 Action 汇总

Data Rule Engine 不暴露 Action 接口。

---

## 5. 内部设计

### 5.1 节点结构

```
Data Rule Engine Node
├── RuleParser（规则解析器）
│   ├── YamlParser（YAML 解析）
│   └── RuleValidator（规则验证）
├── RuleEngine（规则引擎核心）
│   ├── RuleRegistry（规则注册表）
│   ├── TopicSubscriber（Topic 订阅管理）
│   ├── ConditionEvaluator（条件评估器）
│   ├── TriggerDispatcher（触发分发器）
│   └── ConflictArbiter（冲突仲裁器）
├── ActionExecutor（动作执行器）
│   ├── DRCommander（DR 采集指令发送）
│   └── UploadCommander（上传指令发送）
└── ExecutionLogger（执行日志器）
    ├── TriggerLog（触发记录）
    └── ResultLog（结果记录）
```

### 5.2 关键流程

#### 5.2.1 规则热更新流程

```
云端通过 Gateway 下发规则 YAML
    ↓
Gateway 调用 /data_rule_engine/deploy_rules Service
    ↓
RuleParser 解析 YAML，验证规则语法和合法性
    ↓
RuleValidator 检查：
  - 规则 ID 唯一性
  - 条件表达式合法性
  - 引用的 Topic 是否可订阅
  - 优先级范围合法性
    ↓
验证通过 → RuleRegistry 加载/更新规则
    ↓
TopicSubscriber 按需订阅新 Topic，取消无用订阅
    ↓
返回部署结果（成功/失败的规则 ID 列表）
```

#### 5.2.2 规则触发与执行流程

```
TopicSubscriber 收到 Topic 消息 / 状态变化
    ↓
ConditionEvaluator 评估所有 ACTIVE 规则的条件
    ↓
条件满足的规则进入 TRIGGERED 状态
    ↓
ConflictArbiter 检查冲突：
  - 同一规则冷却期未过 → 丢弃
  - 多条规则同时触发 → 按优先级排序
    ↓
TriggerDispatcher 发送触发事件广播
    ↓
ActionExecutor 执行动作：
  - dr_record → 调用 DR Service 开始录制
  - upload → 调用 Data Uploader Service 触发上传
  - both → 先录制，录制完成后自动上传
    ↓
ExecutionLogger 记录执行日志
    ↓
规则进入 COOLING 状态，冷却期结束后恢复 ACTIVE
```

### 5.3 条件表达式语法（白名单约束）

条件表达式仅支持白名单内的运算符，禁止函数调用、属性访问链和字符串拼接。

**允许的操作符**：`==`, `!=`, `<`, `>`, `<=`, `>=`, `IN`, `BETWEEN`, `AND`, `OR`, `NOT`

```
# Topic 条件（只允许字段名 + 比较运算符）
"/hds/diagnosis_event.level >= ERROR"
"/agent/decision_quality.confidence_score < 0.3"
"/sm/robot_state == ACTIVE_WALKING"

# 状态条件
"sm_state IN [ACTIVE_WALKING, ACTIVE_STANDING]"
"hds_level == CRITICAL"

# 时间条件
"time_of_day BETWEEN 02:00 AND 06:00"

# 组合条件
"(/hds/diagnosis_event.level >= ERROR) AND (sm_state != FAULT)"
```

**安全约束**：
- 表达式解析器必须经过 fuzz 测试验证沙箱完整性
- 禁止：函数调用、属性访问链（如 `obj.a.b`）、字符串拼接、赋值操作
- 所有字符串字面量视为常量，不参与动态求值

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 交互内容 | 方向 | 接口类型 |
|------|---------|------|---------|
| Gateway | 规则 YAML 下发 | DRE ← Gateway | Service |
| DR | 采集触发指令 | DRE → DR | Service |
| Data Uploader | 上传触发指令 | DRE → Data Uploader | Service |
| SM | 机器人状态 | DRE ← SM | Topic |
| HDS | 诊断事件 | DRE ← HDS | Topic |
| Agent | 决策置信度 | DRE ← Agent | Topic |
| TE | 任务状态 | DRE ← TE | Topic |
| Setting | 引擎配置 | DRE ← Setting | 参数读取 |
| EM | 进程管理 | DRE ↔ EM | 心跳 + 控制 |

### 6.2 规则触发示例

```yaml
# 示例：失效自动捕获规则
rules:
  - rule_id: "rule_failure_capture"
    name: "failure_capture"
    priority: 10
    trigger_type: "topic"
    trigger_condition: "/hds/diagnosis_event.level >= ERROR"
    action_type: "dr_record"
    action_duration_before_sec: 10
    action_duration_after_sec: 30
    tags: ["failure", "auto_capture"]
    auto_upload: true
    cooldown_sec: 60
    enabled: true

  - rule_id: "rule_vla_low_conf"
    name: "vla_low_confidence"
    priority: 5
    trigger_type: "topic"
    trigger_condition: "/agent/decision_quality.confidence_score < 0.3"
    action_type: "dr_record"
    action_duration_before_sec: 5
    action_duration_after_sec: 15
    tags: ["vla", "low_confidence"]
    auto_upload: false
    cooldown_sec: 30
    enabled: true

  - rule_id: "rule_night_upload"
    name: "night_idle_upload"
    priority: 1
    trigger_type: "time"
    trigger_condition: "time_of_day BETWEEN 02:00 AND 06:00"
    action_type: "upload"
    tags: ["scheduled", "idle_upload"]
    auto_upload: true
    cooldown_sec: 3600
    enabled: true
```

---

## 7. 关键参数与配置

### 7.1 参数文件 `data_rule_engine_params.yaml`

```yaml
data_rule_engine:
  # 引擎配置
  max_rules: 100                   # 最大规则数量
  max_topic_subscriptions: 50      # 最大 Topic 订阅数
  evaluation_interval_ms: 100      # 条件评估间隔（毫秒）
  default_cooldown_sec: 30         # 默认冷却时间（秒）

  # 规则文件
  rules_file_path: "/opt/striding/rules/data_collection_rules.yaml"
  auto_reload_rules: true          # 文件变更时自动重载
  reload_debounce_sec: 5           # 重载防抖时间

  # 触发限制
  max_triggers_per_minute: 20      # 每分钟最大触发次数（全局）
  burst_protection: true           # 突发保护：连续触发 N 次后自动降速

  # 日志
  log_retention_days: 30           # 执行日志保留天数
```

---

## 8. 错误码定义

Data Rule Engine 错误码范围：**8001-8020**

| 码 | 常量 | 说明 | 级别 |
|----|------|------|------|
| 8001 | DRE_ERR_RULE_PARSE_FAIL | 规则 YAML 解析失败 | HIGH |
| 8002 | DRE_ERR_RULE_VALIDATION_FAIL | 规则验证失败（语法/语义） | MEDIUM |
| 8003 | DRE_ERR_TOPIC_SUBSCRIBE_FAIL | Topic 订阅失败 | HIGH |
| 8004 | DRE_ERR_CONDITION_EVAL_FAIL | 条件表达式评估失败 | MEDIUM |
| 8005 | DRE_ERR_DR_COMMAND_FAIL | DR 采集指令发送失败 | HIGH |
| 8006 | DRE_ERR_UPLOAD_COMMAND_FAIL | 上传指令发送失败 | MEDIUM |
| 8007 | DRE_ERR_RULE_CONFLICT | 规则冲突检测异常 | LOW |
| 8008 | DRE_ERR_MAX_RULES_EXCEEDED | 超出最大规则数量限制 | MEDIUM |
| 8009 | DRE_ERR_BURST_PROTECTION | 突发保护已触发 | LOW |
| 8010 | DRE_ERR_RULES_FILE_NOT_FOUND | 规则文件不存在 | HIGH |
| 8011 | DRE_ERR_RULES_FILE_CORRUPTED | 规则文件损坏 | HIGH |
| 8012 | DRE_ERR_DUPLICATE_RULE_ID | 规则 ID 重复 | MEDIUM |

---

## 9. 包结构

```
data_rule_engine/
├── include/data_rule_engine/
│   ├── data_rule_engine_node.hpp
│   ├── rule_parser.hpp
│   ├── rule_engine.hpp
│   ├── rule_registry.hpp
│   ├── condition_evaluator.hpp
│   ├── trigger_dispatcher.hpp
│   ├── conflict_arbiter.hpp
│   └── execution_logger.hpp
├── src/
│   ├── data_rule_engine_node.cpp
│   ├── rule_parser.cpp
│   ├── rule_engine.cpp
│   ├── rule_registry.cpp
│   ├── condition_evaluator.cpp
│   ├── trigger_dispatcher.cpp
│   ├── conflict_arbiter.cpp
│   └── execution_logger.cpp
├── config/
│   └── data_rule_engine_params.yaml
├── launch/
│   └── data_rule_engine.launch.py
├── CMakeLists.txt
└── package.xml

data_rule_msgs/
├── msg/
│   ├── RuleDefinition.msg
│   ├── RuleTriggerEvent.msg
│   ├── RuleExecutionLog.msg
│   └── Heartbeat.msg
├── srv/
│   ├── DeployRules.srv
│   ├── GetActiveRules.srv
│   └── EnableRule.srv
├── CMakeLists.txt
└── package.xml
```

---

## 10. 安全约束

1. **规则沙箱**：条件表达式只允许读取 Topic 字段，禁止调用系统函数或访问文件系统
2. **触发限速**：全局每分钟最大触发次数限制，防止规则风暴导致系统过载
3. **规则验证**：加载规则时必须验证所有引用的 Topic 存在且可订阅，拒绝无效规则
4. **优先级仲裁**：多条规则同时触发时，严格按优先级排序，高优先级规则优先执行
5. **冷却保护**：单条规则触发后必须等待冷却期结束才能再次触发
6. **无状态修改**：规则引擎只读取系统状态，不修改任何模块的内部状态
7. **故障隔离**：单条规则执行失败不影响其他规则的正常运行
