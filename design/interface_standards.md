# 跨模块 ROS2 接口标准规范

> 本规范用于统一全部 23 个模块的 Heartbeat.msg、GetHealthStatus.srv 及通用命名约定。
> 所有设计文档中的接口定义必须符合本规范。

---

## 1. Heartbeat.msg 标准模板

每个模块的 `Heartbeat.msg` 必须包含以下**标准字段**（顺序固定）：

```msg
builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message
```

**字段说明**：

| 字段 | 类型 | 说明 |
|------|------|------|
| `stamp` | `builtin_interfaces/Time` | 心跳产生时间戳 |
| `node_name` | `string` | 节点名称（如 `"mc_node"`、`"sm_node"`） |
| `state` | `uint8` | 模块状态枚举值（模块自定义，0=OK） |
| `healthy` | `bool` | 整体健康：true=正常，false=异常 |
| `status_message` | `string` | 人类可读状态描述（正常时可为空） |

**扩展规则**：模块可在标准字段后追加**模块专属字段**，但标准字段的顺序和类型不得变更。

**示例**（HAL_EtherCAT 扩展）：
```msg
builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message
uint8 bus_state         # 扩展字段
uint16 online_slaves    # 扩展字段
bool dc_synced          # 扩展字段
```

---

## 2. GetHealthStatus.srv 标准模板

每个模块的 `GetHealthStatus.srv` 响应必须包含以下**标准字段**（顺序固定）：

```srv
---
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
```

**字段说明**：

| 字段 | 类型 | 说明 |
|------|------|------|
| `success` | `bool` | 查询是否成功 |
| `node_name` | `string` | 节点名称 |
| `uptime_since` | `builtin_interfaces/Time` | 节点启动时间戳 |
| `state` | `uint8` | 当前状态枚举值 |
| `healthy` | `bool` | 整体健康 |
| `message` | `string` | 状态描述或错误信息 |

**扩展规则**：模块可在 `message` 后追加**模块专属响应字段**。请求部分（`---` 上方）保持为空（无参数查询）。

**示例**（RC 扩展）：
```srv
---
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
uint32 modules_online   # 扩展字段
```

---

## 3. ErrorCode.msg 命名规范

统一命名为 `ErrorCode.msg`（禁止前缀式命名如 `McErrorCode.msg`、`PncErrorCode.msg`）。

**错误码值格式**：
```msg
uint16 OK = 0
uint16 ERR_{MODULE}_{DESCRIPTION} = {value}
```

示例：
```msg
uint16 ERR_MC_BUS_INIT_FAILED = 2001
uint16 ERR_PNC_PLANNING_FAILED = 4001
```

---

## 4. 通用命名规范

### 4.1 Time 字段

统一使用 `stamp`，禁止使用 `timestamp`。

### 4.2 力矩字段

统一使用 `efforts`（与 ROS2 `sensor_msgs/JointState` 一致），禁止使用 `torques`。

### 4.3 Service 命名

必须遵循 `VerbNoun.srv` 格式：
- ✅ `GetHealthStatus.srv`、`SetParameter.srv`、`TriggerEStop.srv`
- ❌ `ListMaps.srv` → 改为 `GetMapList.srv`
- ❌ `DeleteMap.srv` → 改为 `RemoveMap.srv`
- ❌ `CreateTask.srv` → 改为 `SubmitTask.srv`

### 4.4 Service 响应

所有 Service 响应必须包含：
```msg
bool success
string message
uint16 error_code   # 可选，但推荐包含
```

---

## 5. 已修复的阻塞级问题记录

| 问题 | 修复方案 | 状态 |
|------|----------|------|
| ms_msgs/MotionTarget 与 mc_msgs/MotionTarget 同名异构 | ms_msgs 版本重命名为 `StreamMotionTarget` | 已修复 |
| mc_msgs/TriggerEStop 与 sm_msgs/TriggerEStop 冲突 | 删除 mc_msgs 版本，统一使用 sm_msgs | 已修复 |
| hal_audio_msgs StartRecording 与 dr_msgs StartRecording 冲突 | hal_audio 版本重命名为 `StartAudioRecording` | 已修复 |

---

## 6. 待修复问题清单

### Stage 2：统一标准模板
- [x] 统一全部 23 个模块的 Heartbeat.msg（应用标准模板）
- [x] 统一全部 23 个模块的 GetHealthStatus.srv（应用标准模板）
- [x] 统一 ErrorCode.msg 命名（去除前缀式命名）
- [x] 统一 Time 字段命名（stamp 替代 timestamp）
- [x] 统一力矩字段命名（efforts 替代 torques）

### Stage 3：服务命名规范化
- [x] 修复非 VerbNoun 格式的 Service 命名
- [x] 统一 Service 响应字段（success + message + error_code）
