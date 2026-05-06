# Setting 模块设计

## 1. 模块概述与定位

**模块名称**：Setting（设置管理）

**定位**：Setting 是端侧软件系统中的**配置单一数据源**，位于应用层。它提供统一、持久化、带Schema验证的参数存储服务，所有模块的运行时配置均通过Setting读写，确保配置一致性、可追溯性和热更新能力。

**核心职责**：

1. **统一参数存储**：维护全局key-value参数仓库，支持多命名空间隔离
2. **Schema验证**：所有写入参数必须通过预定义的Schema校验（类型、范围、枚举）
3. **持久化**：参数原子写入磁盘（YAML），启动时自动加载
4. **热更新**：支持参数在线修改并实时通知订阅模块，无需重启
5. **云同步**：通过Gateway与云端配置中心双向同步
6. **审计日志**：记录所有参数变更历史（谁、何时、从什么改为什么）
7. **工厂重置**：支持按命名空间或全量恢复出厂默认值
8. **批量操作**：支持原子性批量参数读写

**与相邻模块的边界**：

| 边界 | Setting 负责 | 对方负责 |
|------|-------------|---------|
| Setting ↔ ALL | 提供参数读写接口，下发变更通知 | 各模块读取自身配置并应用 |
| Setting ↔ Gateway | 接收云端配置推送，上报配置变更 | 云端通信、鉴权 |
| Setting ↔ SM | 读取/修改SM相关配置（如E-Stop超时时间） | 状态机决策 |
| Setting ↔ HDS | 上报配置异常（Schema冲突等） | 故障诊断与定级 |
| Setting ↔ FOTA | 读取更新策略配置 | 固件更新执行 |

---

## 2. 职责边界

**Setting 不做的事情**（红线）：

- **不做业务逻辑决策** — 只存储和传递配置值，不根据配置值做业务判断
- **不做运行时状态管理** — 不存储动态运行时变量（如当前关节位置），只存配置参数
- **不做大数据存储** — 不存储日志、遥测、地图数据等，那是DR/MapManager的职责
- **不直接连接云端** — 所有云端配置同步通过Gateway转发
- **不做故障定级** — Schema校验失败等异常上报HDS定级

---

## 3. 内部架构

### 3.1 参数存储状态

Setting本身不维护复杂状态机，但参数有生命周期状态：

| 状态 | 说明 |
|------|------|
| `DEFAULT` | 使用出厂默认值，未被修改 |
| `OVERRIDDEN` | 已被本地修改，与默认值不同 |
| `SYNCED` | 已与云端同步确认 |
| `PENDING_SYNC` | 本地已修改，等待云端同步 |
| `CONFLICT` | 本地与云端版本冲突，需人工解决 |

### 3.2 参数生命周期

```
┌─────────────┐   首次写入    ┌─────────────┐   云端确认    ┌─────────────┐
│   DEFAULT   │─────────────►│  OVERRIDDEN │─────────────►│   SYNCED    │
└─────────────┘              └──────┬──────┘              └─────────────┘
     ▲                              │
     │     恢复出厂                 │  云端推送不同值
     └──────────────────────────────┘
                                    │
                                    ▼
                              ┌─────────────┐
                              │   CONFLICT  │◄────人工选择保留版本──┐
                              └─────────────┘                      │
                                    │                              │
                                    └──────────────────────────────┘
```

---

## 4. ROS2 接口定义

### 4.1 消息定义 (msg)

```
# setting_msgs/msg/ParameterValue.msg
# 带类型的参数值

string name                 # 参数名（含命名空间，如 "mc.joint_limit.max_torque"）
uint8 type                  # 参数类型
uint8 TYPE_STRING  = 0
uint8 TYPE_INT64   = 1
uint8 TYPE_DOUBLE  = 2
uint8 TYPE_BOOL    = 3
uint8 TYPE_STRING_ARRAY = 4
uint8 TYPE_INT64_ARRAY  = 5
uint8 TYPE_DOUBLE_ARRAY = 6
uint8 TYPE_BOOL_ARRAY   = 7
string string_value
int64 int_value
float64 double_value
bool bool_value
string[] string_array
int64[] int_array
float64[] double_array
bool[] bool_array
```

```
# setting_msgs/msg/ParameterSchema.msg
# 参数校验Schema

string name                 # 参数名
uint8 type                  # 期望类型
bool required               # 是否必填
string description          # 参数描述
# 范围约束（仅数值型有效）
float64 min_value
float64 max_value
# 枚举约束（仅字符串型有效）
string[] allowed_values
# 默认值
ParameterValue default_value
string unit                 # 单位（如 "Nm", "Hz", "m/s"）
```

```
# setting_msgs/msg/ParameterChangeEvent.msg
# 参数变更事件

string name                 # 变更的参数名
ParameterValue old_value    # 旧值
ParameterValue new_value    # 新值
string changed_by           # 变更来源（"gateway", "local_service", "default"）
builtin_interfaces/Time changed_at
string reason               # 变更原因
```

```
# setting_msgs/msg/Heartbeat.msg
# Setting 心跳

builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message
uint32 total_parameters
uint32 pending_sync_count
```

```
# setting_msgs/msg/ParameterNamespace.msg
# 命名空间信息

string name                 # 命名空间（如 "mc", "pnc", "gateway"）
string description
uint32 parameter_count
bool is_persistent          # 是否持久化
```

### 4.2 服务定义 (srv)

```
# setting_msgs/srv/GetHealthStatus.srv

# Request（空）
---
# Response
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
uint32 total_parameters
uint32 schema_count
```

```
# setting_msgs/srv/GetParameter.srv
# 读取单个参数

string name                 # 参数名
---
# Response
bool found
ParameterValue value
uint8 source                # 值来源：0=DEFAULT, 1=OVERRIDDEN, 2=SYNCED
string message
```

```
# setting_msgs/srv/SetParameter.srv
# 写入单个参数（带验证）

string name
ParameterValue value
string changed_by           # 变更来源标识
string reason
bool skip_validation        # 是否跳过验证（管理员权限，默认false）
---
# Response
bool success
uint16 error_code
string message
ParameterValue accepted_value  # 实际写入的值（可能被修正）
```

```
# setting_msgs/srv/GetParameterList.srv
# 按命名空间列出参数

string namespace            # 命名空间，空字符串表示全部
bool include_values         # 是否包含值
---
# Response
bool success
ParameterValue[] parameters
uint32 count
```

```
# setting_msgs/srv/SetParametersBatch.srv
# 原子批量写入

ParameterValue[] parameters
string changed_by
string reason
---
# Response
bool success
uint16 error_code
string message
uint32 failed_index         # 第几个参数失败（如果失败）
```

```
# setting_msgs/srv/ResetToDefault.srv
# 恢复出厂设置

string namespace            # 命名空间，空字符串表示全部
bool confirm                # 必须传true，防止误操作
---
# Response
bool success
uint16 error_code
string message
uint32 reset_count
```

```
# setting_msgs/srv/ValidateParameter.srv
# 仅验证，不写入

string name
ParameterValue value
---
# Response
bool valid
uint16 error_code
string message
```

### 4.3 接口汇总表

#### Topics

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/setting/parameter_change_event` | `setting_msgs/msg/ParameterChangeEvent` | Setting → ALL | Reliable + Transient Local + Depth 100 | 事件驱动 | 参数变更通知 |
| `/setting/heartbeat` | `setting_msgs/msg/Heartbeat` | Setting → EM/HDS | Reliable + Volatile + Depth 1 | 1 Hz | 心跳 |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/setting/get_health_status` | `setting_msgs/srv/GetHealthStatus` | EM, HDS | 健康查询 |
| `/setting/get_parameter` | `setting_msgs/srv/GetParameter` | 任意模块 | 读取参数 |
| `/setting/set_parameter` | `setting_msgs/srv/SetParameter` | Gateway, 本地 | 写入参数（带验证） |
| `/setting/get_parameter_list` | `setting_msgs/srv/GetParameterList` | Gateway, 本地 | 列出参数 |
| `/setting/set_parameters_batch` | `setting_msgs/srv/SetParametersBatch` | Gateway | 批量写入 |
| `/setting/reset_to_default` | `setting_msgs/srv/ResetToDefault` | Gateway（管理员） | 恢复出厂 |
| `/setting/validate_parameter` | `setting_msgs/srv/ValidateParameter` | 任意模块 | 仅验证 |

---

## 5. 内部设计

### 5.1 节点架构

```
┌──────────────────────────────────────────────────────────────────────┐
│                        SettingNode                                    │
│                                                                       │
│  ┌──────────────────┐    ┌──────────────────┐                        │
│  │  Parameter Store │    │  Schema Registry │                        │
│  │  (参数仓库)       │    │  (Schema注册表)   │                        │
│  │                  │    │                  │                        │
│  │  - key-value存储  │    │  - 参数定义加载   │                        │
│  │  - 命名空间隔离   │    │  - 写入前校验    │                        │
│  │  - 内存缓存      │    │  - 默认值管理    │                        │
│  │  - 变更历史      │    │  - 范围/枚举检查 │                        │
│  └────────┬─────────┘    └────────┬─────────┘                        │
│           │                       │                                   │
│  ┌────────▼───────────────────────▼─────────┐                        │
│  │         Validation Engine                 │                        │
│  │   (类型检查 → 范围检查 → 枚举检查 → 依赖检查)│                       │
│  └────────┬───────────────────────┬─────────┘                        │
│           │                       │                                   │
│  ┌────────▼─────────┐   ┌─────────▼────────┐                        │
│  │  Persistence     │   │  Change Notifier │                        │
│  │  Manager         │   │  (变更通知器)     │                        │
│  │  (持久化管理)     │   │                  │                        │
│  │  - 原子写入YAML  │   │  - Topic广播     │                        │
│  │  - 启动加载      │   │  - 回调通知      │                        │
│  │  - 备份恢复      │   │  - 差异检测      │                        │
│  └────────┬─────────┘   └──────────────────┘                        │
│           │                                                           │
│  ┌────────▼─────────┐   ┌──────────────────┐                        │
│  │  Cloud Sync      │   │  Audit Logger    │                        │
│  │  Agent           │   │  (审计日志)       │                        │
│  │  (云同步代理)     │   │                  │                        │
│  │  - 拉取云端配置  │   │  - 变更记录      │                        │
│  │  - 推送本地变更  │   │  - 查询接口      │                        │
│  │  - 冲突检测      │   │  - 历史回溯      │                        │
│  └──────────────────┘   └──────────────────┘                        │
│                                                                       │
│  ┌─────────────────────────────────────────┐                        │
│  │  ROS2 Service/Topic Interface           │                        │
│  └─────────────────────────────────────────┘                        │
└──────────────────────────────────────────────────────────────────────┘
```

### 5.2 关键设计决策

1. **原子写入**：参数写入采用"写临时文件 → 重命名"策略，避免写入过程中断电导致文件损坏
2. **内存缓存优先**：所有读取操作从内存缓存返回（<1ms），磁盘只用于持久化和启动加载
3. **Schema严格模式**：默认严格校验，不允许写入未在Schema中定义的参数（防止拼写错误）
4. **变更差异检测**：只有在值真正发生变化时才触发通知，避免无意义的重复通知
5. **命名空间权限**：每个模块只能写入自己的命名空间（如mc模块只能写mc.*），需要跨命名空间写入需特殊权限

### 5.3 关键流程

#### 5.3.1 参数写入流程

```
调用者 → /setting/set_parameter(name, value)
  → Validation Engine:
      1. 检查参数名格式（必须含命名空间前缀）
      2. 检查Schema是否存在（严格模式：不存在则拒绝）
      3. 类型检查（int不能写入string参数）
      4. 范围检查（数值必须在min/max内）
      5. 枚举检查（字符串必须在allowed_values内）
  → Parameter Store 写入内存
  → Persistence Manager 原子写入YAML
  → Audit Logger 记录变更历史
  → Change Notifier:
      → 发布 /setting/parameter_change_event
      → 通知注册了回调的本地组件
  → 返回 success=true
```

#### 5.3.2 启动加载流程

```
SettingNode 启动
  → 读取参数文件 /opt/robot/config/settings.yaml
    → 文件不存在 → 加载内置默认值，创建新文件
    → 文件存在 → 解析所有参数
  → 加载Schema文件 /opt/robot/config/schemas.yaml
  → 校验已有参数是否符合Schema
    → 不符合 → 标记为异常，上报HDS，使用默认值覆盖
  → 参数加载完成，标记READY
  → 各模块启动后通过Service读取自身配置
```

#### 5.3.3 云端同步流程

```
云端配置变更
  → Gateway 转发至 Setting
  → Setting 比较云端值与本地值
    → 相同 → 标记SYNCED，无操作
    → 不同且本地为OVERRIDDEN → 标记CONFLICT
    → 不同且本地为DEFAULT → 写入，标记SYNCED
  → 如有CONFLICT → 发布事件通知APP
  → APP用户选择保留版本
    → 保留本地 → 上报云端覆盖
    → 保留云端 → 本地写入，标记SYNCED
```

#### 5.3.4 工厂重置流程

```
管理员通过APP发起工厂重置
  → Gateway 转发 /setting/reset_to_default
  → Setting 校验 confirm=true
  → 按命名空间（或全部）恢复默认值
  → 每个被重置的参数发布 change_event
  → 持久化更新后的YAML
  → 返回 reset_count
  → 上报云端同步默认值
```

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|------|------|------|------|
| ALL | Setting → ALL | `/setting/parameter_change_event` (Topic) | 参数变更广播 |
| ALL | ALL → Setting | `/setting/get_parameter` (Service) | 各模块读取自身配置 |
| ALL | ALL → Setting | `/setting/validate_parameter` (Service) | 预验证配置 |
| Gateway | Gateway → Setting | `/setting/set_parameter` (Service) | 云端配置推送 |
| Gateway | Setting → Gateway | `/setting/parameter_change_event` (Topic) | 本地变更上报云端 |
| SM | SM → Setting | `/setting/get_parameter` (Service) | 读取状态机超时等配置 |
| HDS | Setting → HDS | `/hds/report_diagnosis` (Service) | 上报Schema冲突等异常 |
| FOTA | FOTA → Setting | `/setting/get_parameter` (Service) | 读取更新策略配置 |
| EM | EM → Setting | `/setting/get_health_status` (Service) | 健康检查 |

### 6.2 关键交互时序

#### 时序1：模块启动时读取配置

```
MC启动              Setting              磁盘
  │                   │                   │
  │──get_parameter───►│                   │
  │  "mc.control_freq"│                   │
  │                   │──内存命中────────►│
  │                   │◄──返回值──────────│
  │                   │                   │
  │◄──ParameterValue──│                   │
  │                   │                   │
  │──get_parameter───►│                   │
  │  "mc.joint_limit" │                   │
  │                   │──内存命中────────►│
  │                   │◄──返回值──────────│
  │◄──ParameterValue──│                   │
```

#### 时序2：用户修改配置并热生效

```
APP用户        Gateway         Setting        MC
  │              │               │             │
  │──改配置─────►│               │             │
  │              │──set_param───►│             │
  │              │               │             │
  │              │               │──验证通过───│
  │              │               │             │
  │              │◄──success────│             │
  │◄──确认───────│               │             │
  │              │               │             │
  │              │               │──change_event──►│
  │              │               │               │
  │              │               │               │──应用新配置──►
```

---

## 7. 关键参数与配置

### 7.1 运行时参数

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `settings_file_path` | string | "/opt/robot/config/settings.yaml" | 参数持久化文件路径 |
| `schemas_file_path` | string | "/opt/robot/config/schemas.yaml" | Schema定义文件路径 |
| `backup_count` | int | 5 | 保留的历史备份数量 |
| `strict_mode` | bool | true | 是否拒绝未定义Schema的参数 |
| `max_parameter_size` | int | 1048576 | 单个参数最大字节数（1MB） |
| `max_total_parameters` | int | 10000 | 最大参数总数 |
| `sync_interval_sec` | int | 300 | 云端同步间隔（秒） |
| `audit_log_max_entries` | int | 10000 | 审计日志最大条目数 |
| `namespace_write_permission` | string[] | ["*"] | 允许写入的命名空间（*表示全部） |

### 7.2 配置文件结构

```yaml
# setting_params.yaml
setting_node:
  ros__parameters:
    storage:
      settings_file: "/opt/robot/config/settings.yaml"
      schemas_file: "/opt/robot/config/schemas.yaml"
      backup_dir: "/opt/robot/config/backups/"
      backup_count: 5
      atomic_write: true

    validation:
      strict_mode: true
      max_parameter_size: 1048576
      max_total_parameters: 10000

    sync:
      enabled: true
      interval_sec: 300
      conflict_resolution: "manual"  # manual / local_wins / cloud_wins

    audit:
      enabled: true
      max_entries: 10000
      log_changed_by: true
      log_old_value: true

    security:
      namespace_permissions:
        - namespace: "*"
          allowed_writers: ["gateway", "local_admin"]
```

---

## 8. 错误码定义

```
# setting_msgs/msg/ErrorCode.msg
uint16 OK                          = 0
uint16 ERR_PARAMETER_NOT_FOUND     = 15001   # 参数不存在
uint16 ERR_INVALID_PARAMETER_TYPE  = 15002   # 参数类型不匹配
uint16 ERR_VALIDATION_FAILED       = 15003   # Schema校验失败
uint16 ERR_PARAMETER_READ_ONLY     = 15004   # 参数只读
uint16 ERR_STORAGE_WRITE_FAILED    = 15005   # 磁盘写入失败
uint16 ERR_NAMESPACE_NOT_FOUND     = 15006   # 命名空间不存在
uint16 ERR_PARAMETER_TOO_LARGE     = 15007   # 参数值过大
uint16 ERR_INVALID_NAMESPACE       = 15008   # 非法命名空间（无权限）
uint16 ERR_SYNC_FAILED             = 15009   # 云端同步失败
uint16 ERR_SCHEMA_NOT_FOUND        = 15010   # Schema定义缺失
```

| 错误码 | 常量名 | 说明 | 严重级别 |
|--------|--------|------|----------|
| 0 | OK | 成功 | — |
| 15001 | ERR_PARAMETER_NOT_FOUND | 参数不存在 | LOW |
| 15002 | ERR_INVALID_PARAMETER_TYPE | 参数类型不匹配 | MEDIUM |
| 15003 | ERR_VALIDATION_FAILED | Schema校验失败 | MEDIUM |
| 15004 | ERR_PARAMETER_READ_ONLY | 参数只读 | MEDIUM |
| 15005 | ERR_STORAGE_WRITE_FAILED | 磁盘写入失败 | HIGH |
| 15006 | ERR_NAMESPACE_NOT_FOUND | 命名空间不存在 | LOW |
| 15007 | ERR_PARAMETER_TOO_LARGE | 参数值过大 | MEDIUM |
| 15008 | ERR_INVALID_NAMESPACE | 非法命名空间 | HIGH |
| 15009 | ERR_SYNC_FAILED | 云端同步失败 | MEDIUM |
| 15010 | ERR_SCHEMA_NOT_FOUND | Schema定义缺失 | MEDIUM |

---

## 9. 安全约束

1. **命名空间隔离**：模块只能写入自己的命名空间，禁止跨命名空间写入（除非有管理员权限）
2. **Schema不可运行时修改**：Schema文件只能由部署流程更新，运行时只读（防止恶意参数注入）
3. **审计不可篡改**：审计日志使用追加写和文件权限保护（只读权限给应用用户）
4. **敏感参数加密**：Token、密钥类参数值在存储时自动AES加密，读取时解密
5. **批量写入原子性**：SetParametersBatch要么全部成功，要么全部失败，不会出现部分写入
6. **工厂重置确认**：ResetToDefault必须传confirm=true，防止误触

---

## 10. 包结构

```
setting_msgs/           # 消息定义包
├── msg/
│   ├── ParameterValue.msg
│   ├── ParameterSchema.msg
│   ├── ParameterChangeEvent.msg
│   ├── Heartbeat.msg
│   └── ParameterNamespace.msg
├── srv/
│   ├── GetHealthStatus.srv
│   ├── GetParameter.srv
│   ├── SetParameter.srv
│   ├── GetParameterList.srv
│   ├── SetParametersBatch.srv
│   ├── ResetToDefault.srv
│   └── ValidateParameter.srv
├── CMakeLists.txt
└── package.xml

setting/                # 节点实现包
├── include/setting/
│   ├── setting_node.hpp
│   ├── parameter_store.hpp
│   ├── schema_registry.hpp
│   ├── validation_engine.hpp
│   ├── persistence_manager.hpp
│   ├── change_notifier.hpp
│   ├── cloud_sync_agent.hpp
│   └── audit_logger.hpp
├── src/
│   ├── setting_node.cpp
│   ├── parameter_store.cpp
│   ├── schema_registry.cpp
│   ├── validation_engine.cpp
│   ├── persistence_manager.cpp
│   ├── change_notifier.cpp
│   ├── cloud_sync_agent.cpp
│   ├── audit_logger.cpp
│   └── main.cpp
├── test/
│   ├── test_validation_engine.cpp
│   ├── test_parameter_store.cpp
│   ├── test_persistence.cpp
│   └── test_integration.cpp
├── config/
│   ├── setting_params.yaml
│   └── default_schemas.yaml
├── launch/
│   └── setting.launch.py
├── CMakeLists.txt
└── package.xml
```

---

## 11. 关键性能指标 (KPI)

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 参数读取延迟 | < 1ms | 从Service调用到返回（内存缓存） |
| 参数写入延迟 | < 10ms | 含验证+持久化（非批量） |
| 批量写入延迟 | < 50ms (100个参数) | 原子批量写入 |
| 启动加载时间 | < 500ms | 从节点启动到可服务 |
| 持久化可靠性 | 100% | 写入后断电不丢失 |
| 内存占用 | < 50MB | 10,000个参数的典型占用 |
| Schema校验覆盖率 | 100% | 严格模式下所有写入必须过Schema |
| 热更新生效延迟 | < 100ms | 从写入到订阅模块收到通知 |
| 审计日志查询延迟 | < 100ms | 最近1000条查询 |
| 云端同步成功率 | > 99.5% | 网络正常时 |
