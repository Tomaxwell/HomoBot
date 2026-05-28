# TF Publisher 模块设计

## 1. 模块概述与定位

**模块名称**：TF Publisher（TF）

**定位**：TF Publisher 是端侧软件系统中的**坐标变换发布与坐标树一致性维护模块**，位于 HAL & Infra 层。它读取机器人 URDF，构建机器人运动学树，订阅关节状态与定位位姿，统一发布 ROS2 标准 `/tf` 与 `/tf_static` 坐标变换，供感知、定位、规划、运动控制、数据记录和调试工具使用。

**核心职责**：

1. 读取并校验 URDF，生成机器人 link/joint 坐标树
2. 发布固定关节、传感器外参等静态坐标变换到 `/tf_static`
3. 根据关节状态发布活动关节动态坐标变换到 `/tf`
4. 按配置转发/发布 `map → odom → base_footprint → base_link` 等基础坐标链
5. 监控坐标树完整性、时间戳新鲜度和 frame 命名冲突（含外部发布者审计）
6. 发布心跳与原始健康指标，供 EM/HDS 监控

**与相邻模块的边界**：

| 边界 | TF 负责 | 对方负责 |
|---|---|---|
| TF ↔ MC | 根据 MC 输出的关节/基座状态发布机器人本体 TF | 状态估计、关节状态滤波、运动控制 |
| TF ↔ VSLAM/Lidar-SLAM | 接收定位位姿并按策略选择 `map/odom/base_link` 变换来源 | 定位、建图、位姿估计 |
| TF ↔ Perception | 提供相机、雷达、机身、足端等 frame 查询基础 | 感知算法、障碍物/语义/地形输出 |
| TF ↔ PnC | 提供规划所需坐标链和时间一致的 base pose | 路径规划、局部控制、避障 |
| TF ↔ MapManager | 接收地图 frame 约定和地图原点配置 | 地图生命周期、地图数据管理 |
| TF ↔ HDS | 上报 TF 断链、时间戳过期、URDF 错误等原始健康数据 | 故障诊断与定级 |
| TF ↔ EM | 被 EM 启停和重启；提供健康状态查询 | 进程生命周期治理 |

**TF 不做的事情**：

- 不做 SLAM、定位融合或状态估计
- 不计算关节控制指令或运动安全决策
- 不直接启动/停止其他模块进程
- 不做故障定级，只上报原始异常与健康指标
- 不直接访问云端或 APP

---

## 2. 职责边界

TF 在端侧架构中的位置：
```plaintext
┌───────────────────────────────────────────────┐
│ Perception / VSLAM / Lidar-SLAM / PnC / DR    │
│   订阅 /tf 与 /tf_static，查询坐标变换          │
├───────────────────────────────────────────────┤
│ TF Publisher（本模块）                         │
│   URDF → frame tree → /tf, /tf_static          │
├───────────────────────────────────────────────┤
│ MC / SLAM / MapManager                         │
│   提供关节状态、基座位姿、地图 frame 配置        │
├───────────────────────────────────────────────┤
│ HAL & Infra                                    │
└───────────────────────────────────────────────┘
```

| 层次 | 模块 | 职责范围 |
|---|---|---|
| 上层 | Perception / PnC / DR / 调试工具 | 查询坐标变换、做感知融合/规划/记录 |
| **本模块** | **TF Publisher** | **读取 URDF，发布标准 TF 坐标树，监控坐标树一致性** |
| 下层/输入 | MC / VSLAM / Lidar-SLAM / MapManager | 提供关节状态、基座/地图位姿、地图 frame 配置 |

### 2.1 坐标系约定
> 【修订】原稿中 `base_footprint` 的父 frame 标注为 `odom`，与 `base_link` 父 frame 相同，导致 odom 同时挂载两个"基座语义 frame"，语义矛盾。修订为标准链路：`odom → base_footprint → base_link`，`base_footprint` 是 `base_link` 在地面投影的中间节点，`base_link` 的父改为 `base_footprint`。

**世界坐标链（World Chain）**

| Frame | 父 Frame | 来源 | 说明 |
|---|---|---|---|
| `map` | 无 | MapManager / SLAM | 全局地图坐标系，全局 TF 树根 |
| `odom` | `map` | VSLAM/Lidar-SLAM 或外部定位桥 | 局部连续里程计坐标系 |
| `base_footprint` | `odom` | TF 投影计算 | `base_link` 在地面的 yaw-only 投影，仅保留 (x, y, yaw)，z/roll/pitch 为 0 |
| `base_link` | `base_footprint` | MC 或 SLAM 位姿 | 机器人基座坐标系，6-DoF |

**机器人本体链（Robot Body Chain）**

| Frame | 父 Frame | 来源 | 说明 |
|---|---|---|---|
| `torso_link` / `pelvis_link` | `base_link` | URDF + 关节状态 | 躯干/骨盆 link |
| `left_foot_link` / `right_foot_link` | URDF 父 link | URDF + 关节状态 | 足端坐标系 |
| `camera_*_frame` | URDF 父 link | URDF fixed joint | 相机外参坐标系 |
| `lidar_*_frame` | URDF 父 link | URDF fixed joint | 雷达外参坐标系 |
| `imu_link` | URDF 父 link | URDF fixed joint | IMU 安装坐标系 |

**完整基础链路示意**：
```plaintext
map
 └── odom
      └── base_footprint          ← TF 投影计算（yaw-only）
           └── base_link          ← MC / SLAM 位姿（6-DoF）
                ├── pelvis_link
                │    ├── left_thigh_link
                │    │    └── ... → left_foot_link
                │    └── right_thigh_link
                │         └── ... → right_foot_link
                ├── torso_link
                │    ├── camera_front_frame  （static）
                │    └── lidar_top_frame     （static）
                └── imu_link                 （static）
```

> **注意**：`publish_base_footprint=false` 时退化为 `odom → base_link`，此时不发布 `base_footprint`，下游需知晓。

### 2.2 坐标发布原则

1. **URDF 是机器人本体 frame 权威来源**：link/joint 名称、父子关系、固定外参均来自 URDF。
2. **关节状态是动态关节角权威来源**：默认使用 `/mc/whole_body_state`，可配置使用标准 `/joint_states` 或 `/hal_ethercat/joint_states`。
3. **定位模块是全局位姿权威来源**：TF 只按配置转发或重命名定位结果，不融合定位。
4. **同一 child frame 只能有一个发布者**：启动时通过 Global TF Observer 订阅全局 `/tf` 与 `/tf_static`，主动检测外部节点（SLAM、robot_state_publisher 等）是否已发布同名 child frame；检测到冲突则上报 `ANOMALY_FRAME_CONFLICT` 并拒绝发布该 frame。
5. **静态与动态分离**：fixed joint 和传感器外参只发布到 `/tf_static`；活动关节和基座位姿发布到 `/tf`。
> 【修订】原稿第 10 条"启动时检查 TF 发布白名单"未给出信息来源，仅依靠本节点自身已知 URDF 无法检测外部节点是否重复发布。修订为依赖 Global TF Observer（内置 tf2 buffer）实现运行时审计。

---

## 3. 状态机设计

### 3.1 TF 内部运行状态

| 状态 | 值 | 说明 |
|---|---|---|
| `TF_INIT` | 0 | 节点初始化中，参数尚未加载完成 |
| `TF_LOADING_MODEL` | 1 | 正在读取并解析 URDF |
| `TF_WAITING_INPUT` | 2 | URDF 已加载，等待关节状态或定位输入 |
| `TF_PUBLISHING` | 3 | 正常发布 `/tf` 与 `/tf_static` |
| `TF_RELOADING` | 4 | 正在热加载 URDF/参数 |
| `TF_DEGRADED` | 5 | 非核心输入缺失或坐标链不完整，核心链可用，发布可用部分 |
| `TF_ERROR` | 6 | URDF 无效、frame 冲突或核心坐标链不可用，停止发布 |

### 3.2 状态转换图
```plaintext
startup
   │
   ▼
┌─────────┐ params_ok ┌──────────────────┐ urdf_ok ┌──────────────────┐
│ TF_INIT │──────────►│ TF_LOADING_MODEL │────────►│ TF_WAITING_INPUT │
└────┬────┘           └────────┬─────────┘         └────────┬─────────┘
     │                         │ urdf_error                 │ input_ready
     │                         ▼                            ▼
     │                    ┌──────────┐                ┌───────────────┐
     └───────────────────►│ TF_ERROR │◄───────────────│ TF_PUBLISHING │
                          └────▲─────┘ fatal_error    └───────┬───────┘
                               │                              │
                               │ reload_failed                │ non-core input_timeout /
                               │                              │ non-core chain_broken
                         ┌─────┴──────┐ reload_request ┌──────▼──────┐
                         │TF_RELOADING│◄───────────────│ TF_DEGRADED │
                         └─────┬──────┘                └──────┬──────┘
                               │ reload_ok                    │ recovered
                               └──────────────────────────────►
                                                              ▼
                                                       TF_PUBLISHING
```

> 【修订】`TF_DEGRADED` 转换条件增加"非核心"限定词，明确只有非核心输入超时或部分链路断链才触发 DEGRADED；核心链路（`odom → base_footprint → base_link`）异常直接进入 `TF_ERROR`。

### 3.3 状态转换约束

1. **URDF 解析失败必须进入**`**TF_ERROR**`，不允许发布不完整或猜测得到的坐标树。
2. **核心坐标链缺失进入**`**TF_ERROR**`：`publish_world_frames=true` 时，`map/odom/base_footprint/base_link` 任一核心 frame 配置冲突或来源缺失即报错。
3. **非核心输入超时进入**`**TF_DEGRADED**`：例如某个手臂关节状态过期，可继续发布其他可用 link，并上报 HDS 原始健康数据。
4. **热加载必须原子切换**：新 URDF 校验通过后才替换运行中的 kinematic tree；失败时保持旧模型。详见 §3.4。
5. `**TF_ERROR**`**状态下停止发布动态 TF**：静态 TF 保留最后已发布状态（不重新广播），便于 tf2 listener 继续查询已缓存变换；但 `TfState` 必须反映错误状态。
6. **TF 不触发 SM 状态转换**：严重错误只通过心跳/健康上报给 HDS/EM，由 HDS/EM 决策。

### 3.4 热加载约束（新增）
> 【修订】原稿未定义热加载允许的变更类型，ROS2 静态 TF 无"撤销"机制，旧 static frame 一经发布即永久存在于 listener 的缓冲区，需要在设计层面加以限制。

热加载（`/tf/reload_robot_description`）允许的变更类型：

| 变更类型 | 是否允许 | 处理方式 |
|---|---|---|
| 更新已有 fixed joint 的 transform 值（外参标定更新） | **允许** | 重新发布 `/tf_static`，新值覆盖旧值 |
| 新增 fixed joint / link | **允许** | 追加发布新 static 边 |
| 更新活动关节运动学参数（限位、轴向） | **允许** | 原子替换 DynamicJointMap |
| 新增活动关节 | **允许** | 追加进入 DynamicJointMap |
| **删除已有 fixed joint（child frame 消失）** | **禁止** | 返回 `ERR_TF_RELOAD_REJECTED`，需重启节点；旧 frame 残留于全局 TF 系统中 |
| **重命名已有 static child frame** | **禁止** | 返回 `ERR_TF_RELOAD_REJECTED`，需重启节点 |
| 删除活动关节 | **允许（有限制）** | 从 DynamicJointMap 移除；若该关节曾因 hold_last 发布过动态 TF，必须上报 `ANOMALY_JOINT_MISSING` |
| 更改 `robot_root_frame` | **禁止** | 返回 `ERR_TF_RELOAD_REJECTED` |

---

## 4. ROS2 接口定义

### 4.1 消息定义（msg）
```plaintext
# tf_msgs/msg/TfState.msg
# TF Publisher 模块状态

uint8 TF_INIT          = 0
uint8 TF_LOADING_MODEL = 1
uint8 TF_WAITING_INPUT = 2
uint8 TF_PUBLISHING    = 3
uint8 TF_RELOADING     = 4
uint8 TF_DEGRADED      = 5
uint8 TF_ERROR         = 6

uint8 state
uint8 previous_state
string robot_model_name
string urdf_source
string robot_root_frame         # 【修订】原 root_frame，改为 robot_root_frame，表示 URDF 机器人本体树根（如 base_link）
string world_root_frame         # 【新增】全局 TF 树根（如 map 或 odom，取决于 publish_world_frames 配置）
uint32 frame_count
uint32 dynamic_joint_count
uint32 static_transform_count
uint32 published_transform_count
float32 max_transform_age_ms
bool frame_tree_complete         # 本模块负责的边全部正常发布
bool global_chain_complete       # 【新增】通过 Global TF Observer 观测到的全局链路是否完整
string status_message
builtin_interfaces/Time stamp
```

```plaintext
# tf_msgs/msg/FrameEdge.msg
# 坐标树中的一条父子边

string parent_frame
string child_frame
string joint_name
uint8 joint_type              # 0=fixed, 1=revolute, 2=continuous, 3=prismatic
                              # 4=floating（不支持，解析时 reject）
                              # 5=planar（不支持，解析时 reject）
bool is_static
bool is_published
bool is_external               # 【新增】true 表示该边由外部节点发布，TF 仅通过 observer 观测到
float32 last_update_age_ms
string source                  # urdf | mc | joint_states | vslam | lidar_slam | map_manager | external
```

```plaintext
# tf_msgs/msg/FrameGraph.msg
# TF 坐标树摘要，用于监控和调试

string root_frame
string[] frame_names
tf_msgs/FrameEdge[] edges
string[] missing_frames
string[] duplicate_child_frames
uint32 disconnected_subtree_count
bool is_local_tree_valid         # 【修订】原 is_tree_valid；本模块发布的子图是否合法
bool is_global_chain_valid       # 【新增】通过 Global TF Observer 观测到全局 required_frames 链是否完整
string validation_message
builtin_interfaces/Time stamp
```

```plaintext
# tf_msgs/msg/TransformAnomaly.msg
# TF 原始异常事件，不做故障定级

uint8 ANOMALY_URDF_INVALID          = 1
uint8 ANOMALY_FRAME_CONFLICT        = 2
uint8 ANOMALY_INPUT_TIMEOUT         = 3
uint8 ANOMALY_CHAIN_DISCONNECTED    = 4
uint8 ANOMALY_TIMESTAMP_JUMP        = 5
uint8 ANOMALY_JOINT_MISSING         = 6

uint8 ANOMALY_RATE_DROP             = 7
uint8 ANOMALY_STATIC_RELOAD_BLOCKED = 8   # 【新增】热加载因含禁止变更类型而被拒绝

uint8 anomaly_type
string frame_id
string child_frame_id
string source
float32 observed_value
float32 threshold_value
string message
builtin_interfaces/Time stamp
```

```plaintext
# tf_msgs/msg/Heartbeat.msg
# TF 模块心跳

builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy                        # 见 §4.5 健康判定规则
uint8 health_detail                 # 【新增】0=OK, 1=DEGRADED_NON_CORE, 2=CORE_CHAIN_MISSING, 3=ERROR
string status_message
bool urdf_loaded
bool frame_tree_complete
bool global_chain_complete          # 【新增】
uint32 published_transform_count
uint32 input_timeout_count
```

```plaintext
# tf_msgs/msg/ErrorCode.msg
# TF 错误码定义

uint16 OK                             = 0
uint16 ERR_TF_URDF_NOT_FOUND          = 6001   # URDF 文件或 robot_description 参数不存在
uint16 ERR_TF_URDF_PARSE_FAILED       = 6002   # URDF 解析失败
uint16 ERR_TF_EMPTY_KINEMATIC_TREE    = 6003   # URDF 中没有有效 link/joint
uint16 ERR_TF_ROOT_FRAME_MISMATCH     = 6004   # URDF robot_root_frame 与配置不一致
uint16 ERR_TF_FRAME_NAME_CONFLICT     = 6005   # child frame 被多个来源发布（本节点或外部节点）
uint16 ERR_TF_JOINT_STATE_TIMEOUT     = 6006   # 关节状态输入超时
uint16 ERR_TF_BASE_POSE_TIMEOUT       = 6007   # 基座/定位位姿输入超时
uint16 ERR_TF_MISSING_REQUIRED_FRAME  = 6008   # 必需 frame 缺失
uint16 ERR_TF_TRANSFORM_TIMESTAMP     = 6009   # 时间戳跳变、回退或过期
uint16 ERR_TF_RELOAD_REJECTED         = 6010   # 热加载校验失败或含禁止变更类型
uint16 ERR_TF_STATIC_TF_PUBLISH_FAIL  = 6011   # 静态 TF 发布失败
uint16 ERR_TF_DYNAMIC_TF_RATE_DROP    = 6012   # 动态 TF 发布频率低于阈值
uint16 ERR_TF_UNSUPPORTED_JOINT_TYPE  = 6013   # 【新增】URDF 中含不支持的 joint 类型（floating/planar）

uint16 error_code
string message
```

### 4.2 服务定义（srv）
```plaintext
# tf_msgs/srv/ReloadRobotDescription.srv
# 热加载 URDF/robot_description

string requester_node
string urdf_source              # 为空表示重新读取当前配置
bool dry_run                    # true=只校验不切换
---
bool success
uint16 error_code
string message
uint32 frame_count
uint32 dynamic_joint_count
string[] rejected_changes       # 【新增】dry_run 或校验失败时，列出被拒绝的变更项说明
```

```plaintext
# tf_msgs/srv/GetTfStatus.srv
# 查询 TF 模块状态（请求体为空）
---
bool success
string message
tf_msgs/TfState state
tf_msgs/FrameGraph frame_graph
```

```plaintext
# tf_msgs/srv/GetFrameGraph.srv
# 获取坐标树摘要

bool include_static
bool include_dynamic
bool include_external            # 【新增】是否包含由 Global TF Observer 观测到的外部边
---
bool success
string message
tf_msgs/FrameGraph frame_graph
```

```plaintext
# tf_msgs/srv/ValidateFrameChain.srv
# 校验两个 frame 之间是否存在可用坐标链

string source_frame
string target_frame
builtin_interfaces/Time query_time
float32 max_age_ms
---
bool success
string message
bool chain_available
string[] chain_frames
float32 newest_transform_age_ms
uint16 error_code
```

```plaintext
# tf_msgs/srv/GetHealthStatus.srv
# 健康状态查询，供 EM/HDS 调用
---
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy                     # 见 §4.5 健康判定规则
uint8 health_detail              # 【新增】同 Heartbeat.health_detail
string message
uint32 frame_count
uint32 input_timeout_count
bool global_chain_complete       # 【新增】
```

### 4.3 Action 定义

TF Publisher 不提供 Action。URDF 热加载和坐标链校验均为短耗时同步操作，使用 Service 即可；长期运行状态通过 Topic 广播。

### 4.4 接口汇总表

#### Topics（发布）

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|---|---|---|---|---|---|
| `/tf` | `tf2_msgs/msg/TFMessage` | TF → ALL | Best Effort, Volatile, Depth 100 | 50-200Hz | 动态坐标变换 |
| `/tf_static` | `tf2_msgs/msg/TFMessage` | TF → ALL | Reliable + Transient Local, Depth 1 | 启动/重载时 | 静态坐标变换 |
| `/tf/tf_state` | `tf_msgs/msg/TfState` | TF → ALL | Reliable + Volatile, Depth 1 | 1Hz | TF 模块状态 |
| `/tf/frame_graph` | `tf_msgs/msg/FrameGraph` | TF → DR/UI/HDS | Reliable + Transient Local, Depth 1 | 1Hz / 事件驱动 | 坐标树摘要（含外部边） |
| `/tf/transform_anomaly` | `tf_msgs/msg/TransformAnomaly` | TF → HDS/DR | Reliable + Volatile, Depth 20 | 事件驱动 | TF 原始异常 |
| `/tf/heartbeat` | `tf_msgs/msg/Heartbeat` | TF → EM/HDS | Reliable + Volatile, Depth 1 | 1Hz | 心跳 |
| `/hds/health_report` | `hds_msgs/msg/HealthReport` | TF → HDS | Reliable + Volatile, Depth 10 | 1Hz / 事件驱动 | 原始健康指标上报 |

#### Topics（订阅）

| 名称 | 类型 | 来源 | QoS | 频率 | 说明 |
|---|---|---|---|---|---|
| `/mc/whole_body_state` | `mc_msgs/msg/WholeBodyState` | MC | Best Effort, Depth 1 | 100-1000Hz | 默认关节状态与基座姿态来源 |
| `/joint_states` | `sensor_msgs/msg/JointState` | 可选标准关节状态源 | Best Effort, Depth 10 | 50-1000Hz | 兼容标准 ROS 工具链 |
| `/hal_ethercat/joint_states` | `hal_ethercat_msgs/msg/JointState` | HAL_EtherCAT | Best Effort, Depth 1 | 1kHz | 可选低层关节原始状态源 |
| `/vslam/pose` | `geometry_msgs/msg/PoseWithCovarianceStamped` | VSLAM | Best Effort, Depth 1 | 30Hz | 可选视觉定位位姿 |
| `/lidar_slam/pose` | `geometry_msgs/msg/PoseWithCovarianceStamped` | Lidar-SLAM | Best Effort, Depth 1 | 20Hz | 可选激光定位位姿 |
| `/map_manager/map_origin` | `geometry_msgs/msg/PoseStamped` | MapManager | Reliable + Transient Local, Depth 1 | 事件驱动 | 地图原点/地图 frame 配置 |
| `/sm/robot_state` | `sm_msgs/msg/RobotState` | SM | Reliable + Transient Local, Depth 1 | 事件驱动 | 用于停机/故障时标记状态，不参与运动决策 |
| `/tf`（只读） | `tf2_msgs/msg/TFMessage` | ALL | Best Effort, Depth 100 | — | 【新增】Global TF Observer 订阅，用于外部冲突审计 |
| `/tf_static`（只读） | `tf2_msgs/msg/TFMessage` | ALL | Reliable + Transient Local, Depth 1 | — | 【新增】Global TF Observer 订阅，用于外部冲突审计 |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|---|---|---|---|
| `/tf/reload_robot_description` | `tf_msgs/srv/ReloadRobotDescription` | EM / 调试工具 | 热加载或校验 URDF |
| `/tf/get_tf_status` | `tf_msgs/srv/GetTfStatus` | EM / HDS / 调试工具 | 查询 TF 状态 |
| `/tf/get_frame_graph` | `tf_msgs/srv/GetFrameGraph` | DR / UI / 调试工具 | 获取坐标树摘要 |
| `/tf/validate_frame_chain` | `tf_msgs/srv/ValidateFrameChain` | Perception / PnC / 调试工具 | 校验 frame 链可达性 |
| `/tf/get_health_status` | `tf_msgs/srv/GetHealthStatus` | EM / HDS | 健康查询 |

#### Actions

无。TF 不提供 Action。

### 4.5 健康判定规则（新增）
> 【新增】原稿 `healthy` 字段仅为布尔值，未定义各状态下的判定规则，导致 HDS/EM 接入时语义不一致。本节明确各场景下 `healthy` 和 `health_detail` 的赋值规则。

| 场景 | TF 状态 | `healthy` | `health_detail` | 说明 |
|---|---|---|---|---|
| 正常发布，核心链完整 | `TF_PUBLISHING` | `true` | `0` (OK) | 全绿 |
| 非核心关节超时，核心链完整 | `TF_DEGRADED` | `true` | `1` (DEGRADED_NON_CORE) | 核心功能正常，可降级运行 |
| 核心链缺失或超时（`publish_world_frames=false` 时以本体链为准） | `TF_ERROR` | `false` | `2` (CORE_CHAIN_MISSING) | 需 HDS/EM 介入 |
| URDF 无效、frame 冲突等致命错误 | `TF_ERROR` | `false` | `3` (ERROR) | 需重启节点 |
| 热加载中（不超过 `reload_service_timeout_sec`） | `TF_RELOADING` | `true` | `0` (OK) | 短暂重载，旧模型仍在发布 |
| 等待必要输入（启动后超过 `base_pose_timeout_ms`） | `TF_WAITING_INPUT` | `false` | `2` (CORE_CHAIN_MISSING) | 必要输入尚未到达 |
| E-Stop/FAULT 状态，输入已冻结 | `TF_DEGRADED` 或 `TF_ERROR` | 取决于核心链是否可用 | 如上规则 | 详见 §9.2 |

---

## 5. 内部设计

### 5.1 节点架构
```plaintext
┌─────────────────────────────────────────────────────────────────────┐
│                      CodexTfPublisherNode                           │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                  Model Manager                              │   │
│  │  - 读取 robot_description / URDF 文件                        │   │
│  │  - 解析 link/joint/tree                                      │   │
│  │  - 校验 robot_root_frame、重复 child frame、joint 类型        │   │
│  │  - 生成 StaticTransformCache / DynamicJointMap               │   │
│  └──────────────────────────────┬──────────────────────────────┘   │
│                                 │                                  │
│                                 ▼                                  │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                  Input Adapter Layer                         │   │
│  │  - McWholeBodyStateAdapter                                  │   │
│  │  - SensorJointStateAdapter                                  │   │
│  │  - EthercatJointStateAdapter                                │   │
│  │  - SlamPoseAdapter                                           │   │
│  └──────────────────────────────┬──────────────────────────────┘   │
│                                 │                                  │
│                                 ▼                                  │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                  Transform Builder                           │   │
│  │  - joint position → link transform                           │   │
│  │  - base pose → odom/base_footprint/base_link transform       │   │
│  │  - base_link → base_footprint yaw-only projection            │   │
│  │  - timestamp policy / stale transform filtering              │   │
│  └──────────────────────────────┬──────────────────────────────┘   │
│                                 │                                  │
│                                 ▼                                  │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                  TF Broadcaster                              │   │
│  │  - StaticTransformBroadcaster → /tf_static                   │   │
│  │  - TransformBroadcaster → /tf                                │   │
│  │  - rate limiter / batching                                   │   │
│  └──────────────────────────────┬──────────────────────────────┘   │
│                                 │                                  │
│                                 ▼                                  │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                  Frame Graph Monitor                         │   │
│  │  - Local Graph：本模块已发布边的完整性校验                    │   │
│  │  - 输入超时/时间跳变检测                                     │   │
│  │  - 异常事件与健康指标发布                                    │   │
│  └──────────────────────────────┬──────────────────────────────┘   │
│                                 │                                  │
│                                 ▼                                  │
│  ┌─────────────────────────────────────────────────────────────┐   │

│  │            Global TF Observer（新增）                        │   │
│  │  - 订阅全局 /tf 与 /tf_static（只读，不发布）                │   │
│  │  - 检测外部节点是否发布与本节点相同的 child frame             │   │
│  │  - 观测 required_frames 全局可达性（含外部发布的边）          │   │
│  │  - 向 Frame Graph Monitor 提供 global_chain_complete 指标    │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  ROS2 Callback Groups:                                              │
│  - cb_group_input:   高频输入订阅（WholeBodyState / JointState 等） │
│  - cb_group_publish: 定时 TF 发布                                   │
│  - cb_group_service: reload/status/validate 服务                    │
│  - cb_group_observer: Global TF Observer 订阅（独立，低优先级）     │
└─────────────────────────────────────────────────────────────────────┘
```

### 5.2 支持的 Joint 类型（新增）
> 【新增】原稿未明确 URDF joint 类型的支持范围，实现时易产生歧义。

| joint_type | 值 | 支持 | 处理方式 |
|---|---|---|---|
| fixed | 0 | **支持** | 发布到 `/tf_static` |
| revolute | 1 | **支持** | 发布到 `/tf`，使用关节角计算旋转 |
| continuous | 2 | **支持** | 发布到 `/tf`，角度无限制，不做包络截断 |
| prismatic | 3 | **支持** | 发布到 `/tf`，使用关节位移计算平移 |
| floating | 4 | **不支持** | URDF 解析时返回 `ERR_TF_UNSUPPORTED_JOINT_TYPE`，进入 `TF_ERROR` |
| planar | 5 | **不支持** | URDF 解析时返回 `ERR_TF_UNSUPPORTED_JOINT_TYPE`，进入 `TF_ERROR` |
| mimic | — | **不支持** | URDF 中 mimic 标签被忽略，mimic 关节按独立关节处理；如 mimic 关节无独立状态输入，触发 `ANOMALY_JOINT_MISSING` |

> **理由**：floating joint 多 DoF，不能从单一 joint position 标量直接推算；planar joint 语义不明确。如未来有支持需求，应以独立关节链路覆盖，而非复用此框架。

### 5.3 关键设计决策

1. **优先复用 ROS2 标准 TF 生态**：对外发布标准 `tf2_msgs/msg/TFMessage`，业务模块通过 tf2 buffer/listener 查询，不引入私有坐标查询协议。
2. **TF 发布与运动控制解耦**：TF 发布频率低于 MC 1kHz 控制环，默认 100Hz；TF 丢帧不得阻塞 MC。
3. **URDF 原子热加载，变更类型受限**：热加载先在影子模型中解析校验，通过后一次性替换；静态 child frame 不得删除或重命名，否则 reject。
4. **输入源可配置但同类只允许一个权威源**：关节状态只能选择 `mc`、`joint_states`、`ethercat_hal` 之一；base pose source 只能选择一个；避免同一 child frame 多发布。
5. **时间戳策略显式化**：动态 TF 默认使用输入消息时间戳；输入时间戳缺失或过期时，可配置使用 node time，但必须上报 anomaly；E-Stop/FAULT 冻结时保留最后有效时间戳，不刷新为 node time。
6. **坐标树监控只输出原始事实**：TF 只报告断链、超时、冲突等，不决定系统进入 `DEGRADED` 或 `FAULT`。
7. **Global TF Observer 独立回调组**：观测线程与发布线程完全解耦，Observer 故障不影响 TF 发布主路径。

### 5.4 输入语义矩阵（新增）
> 【新增】原稿允许多种 base_pose_source，但未定义各来源消息的 frame 语义，导致 TF 不确定该发布哪一段变换。

`base_pose_source` 各来源输入语义约定：

| source 值 | 订阅 topic | 消息类型 | 期望 `header.frame_id` | 期望 `child_frame_id`（隐式） | TF 发布的变换 |
|---|---|---|---|---|---|
| `lidar_slam` | `/lidar_slam/pose` | `PoseWithCovarianceStamped` | `odom` | `base_link` | `odom → base_link`（如启用 base_footprint，则拆分为 `odom → base_footprint` + `base_footprint → base_link`） |
| `vslam` | `/vslam/pose` | `PoseWithCovarianceStamped` | `odom` | `base_link` | 同上 |
| `mc` | `/mc/whole_body_state` | `WholeBodyState` | `odom` | `base_link` | 同上；关节状态与基座位姿来自同一消息，时间戳一致性最优 |
| `none` | — | — | — | — | 不发布世界链；`publish_odom_to_base` 必须同时为 `false` |

> **协方差处理**：TF 模块将 pose 直接转为 `geometry_msgs/TransformStamped`，协方差字段不透传（TF2 标准变换不携带协方差）；下游若需协方差，应直接订阅原始 pose topic。

`map_origin_source` 各来源语义：

| source 值 | 说明 | 发布的变换 |
|---|---|---|
| `map_manager` | 订阅 `/map_manager/map_origin`，期望 `frame_id=world` 或 `frame_id=map` | `map → odom`（仅 `publish_map_to_odom=true` 时） |
| `parameter` | 从参数 `map_origin_pose` 读取静态 pose | `map → odom`，作为静态变换发布到 `/tf_static` |

### 5.5 关键流程

#### 5.5.1 启动流程
```plaintext
EM 启动 TF 进程
  → 声明并读取参数
  → 状态切换 TF_LOADING_MODEL
  → 从 robot_description 参数或 URDF 文件读取模型
  → 解析 URDF：
      1. 校验 link/joint 非空（否则 ERR_TF_EMPTY_KINEMATIC_TREE）
      2. 校验 robot_root_frame 与 URDF 根 link 一致（否则 ERR_TF_ROOT_FRAME_MISMATCH）
      3. 校验 joint 类型仅含支持类型（否则 ERR_TF_UNSUPPORTED_JOINT_TYPE）
      4. 校验 child frame 命名唯一（否则 ERR_TF_FRAME_NAME_CONFLICT）
      5. 拆分 fixed joint 与 movable joint
  → 发布 /tf_static（fixed joint、传感器外参）
  → 启动 Global TF Observer，订阅全局 /tf 与 /tf_static
  → 订阅配置的关节状态源、定位源、SM 状态
  → 状态切换 TF_WAITING_INPUT
  → 收到必要输入后进入 TF_PUBLISHING
  → 周期发布 /tf/heartbeat、/tf/tf_state、/tf/frame_graph
```

#### 5.5.2 动态 TF 发布流程
```plaintext
publish_timer 每 10ms（默认 100Hz）执行：
  1. 读取最新关节状态缓存
  2. 检查输入时间戳是否超过 joint_state_timeout_ms
  3. 遍历 DynamicJointMap：
      - 从关节状态中查找 joint position
      - 按 URDF joint origin + axis 计算 parent → child transform
      - 缺失关节按策略处理：
          strict:    不发布该 subtree，相应 frame 进入 missing；若为必需 frame 则 TF_DEGRADED
          hold_last: 使用上一次合法值（≤ hold_last_max_duration_ms），超时后转 strict；上报 anomaly
  4. 读取定位/基座位姿缓存（如启用）
  5. 生成 odom→base_footprint→base_link 变换（或 odom→base_link，取决于配置）
  6. 如 publish_map_to_odom=true，生成 map→odom 变换
  7. 批量发布 tf2_msgs/TFMessage 到 /tf
  8. 更新 FrameGraphMonitor 指标（local）
```

> 【修订】移除 `zero` 策略，仅保留 `strict` 与 `hold_last`。`zero` 策略在人形机器人中会导致关节链路瞬间跳变，危及感知/规划，仅在 debug 模式可用（见 §7 参数说明）。

#### 5.5.3 URDF 热加载流程
```plaintext
EM/调试工具调用 /tf/reload_robot_description
  → Service 回调进入 TF_RELOADING
  → 读取新 URDF 或 robot_description
  → 在影子模型中解析并完整校验（含 joint 类型、root frame 校验）
  → 比较影子模型与当前模型的变更集，检查是否含禁止变更类型（§3.4）
      - 含禁止变更类型 → 返回 ERR_TF_RELOAD_REJECTED + rejected_changes
      - 继续执行校验
  → dry_run=true：
      - 返回校验结果和变更集说明，不替换当前模型
  → dry_run=false 且校验通过且无禁止变更：
      - 暂停 publish_timer 一个周期（≤ 10ms）
      - 原子替换 DynamicJointMap 与 frame graph
      - 重新发布 /tf_static（追加新 static 边，不发布被删除的 child frame）
      - 清空旧 joint cache 中不再存在的关节
      - 恢复 TF_PUBLISHING 或 TF_WAITING_INPUT
  → 校验失败：
      - 保持旧模型继续运行
      - 发布 ANOMALY_STATIC_RELOAD_BLOCKED 或通用 TransformAnomaly
      - 返回 success=false，error_code=ERR_TF_RELOAD_REJECTED
```

#### 5.5.4 坐标树异常处理流程
```plaintext
FrameGraphMonitor 周期检查（Local Graph）：
  - required_frames 是否全部被本节点发布？
  - 本节点 child_frame 是否与外部节点冲突（来自 Global TF Observer）？
  - 动态 transform age 是否超过阈值？
  - /tf 发布频率是否低于 min_dynamic_publish_rate_hz？
  - 时间戳是否发生大幅回跳（> max_timestamp_jump_ms）？

Global TF Observer 周期检查（Global Graph）：
  - required_frames 在全局 TF 树中是否可达（含外部发布的边）？
  - 是否存在外部节点发布与本节点 child frame 相同的变换？

异常出现：
  → 发布 /tf/transform_anomaly（含 anomaly_type 和具体 frame 信息）
  → 更新 /tf/tf_state.status_message
  → 发布 /hds/health_report 原始指标
  → 非核心异常 → TF_DEGRADED
  → 核心链路异常（world chain 或 required_frames 缺失）→ TF_ERROR
```

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|---|---|---|---|
| MC | MC → TF | `/mc/whole_body_state` | 默认关节状态、基座姿态输入 |
| HAL_EtherCAT | HAL_EtherCAT → TF | `/hal_ethercat/joint_states` | 可选关节原始状态输入 |
| VSLAM | VSLAM → TF | `/vslam/pose` | 可选视觉定位输入；期望 `header.frame_id=odom` |
| Lidar-SLAM | Lidar-SLAM → TF | `/lidar_slam/pose` | 可选激光定位输入；期望 `header.frame_id=odom` |
| MapManager | MapManager → TF | `/map_manager/map_origin` | 地图 frame/原点配置 |
| SM | SM → TF | `/sm/robot_state` | 运行状态上下文，用于健康标记 |
| Perception | TF → Perception | `/tf`, `/tf_static` | 传感器到机体/地图坐标变换 |
| PnC | TF → PnC | `/tf`, `/tf_static` | 规划坐标链查询 |
| DR | TF → DR | `/tf`, `/tf_static`, `/tf/frame_graph` | 数据记录与离线回放 |
| HDS | TF → HDS | `/tf/heartbeat`, `/tf/transform_anomaly`, `/hds/health_report` | 心跳与原始健康指标 |
| EM | EM → TF | 进程启停、`/tf/get_health_status` | 生命周期管理和健康查询 |
| 调试工具 | 调试工具 → TF | `/tf/reload_robot_description`, `/tf/get_frame_graph` | 模型热加载与坐标树调试 |
| Global TF（自订阅） | TF → TF（只读） | `/tf`, `/tf_static` | 【新增】Global TF Observer 观测，不发布 |

### 6.2 启动时序
```plaintext
EM          TF          MC        SLAM/MapManager      Perception/PnC
 │           │           │              │                    │
 │ start TF  │           │              │                    │
 ├──────────►│           │              │                    │
 │           │ load URDF │              │                    │
 │           ├───────────┤              │                    │
 │           │ publish /tf_static       │                    │
 │           ├───────────────────────────────────────────────►│
 │           │ start Global TF Observer │                    │
 │           ├──────────────────────────┤                    │
 │           │ subscribe inputs          │                    │
 │           ├──────────►│              │                    │
 │           ├─────────────────────────►│                    │
 │           │           │ whole_body_state                  │
 │           │◄──────────┤              │                    │
 │           │           │              │ pose/map_origin     │
 │           │◄─────────────────────────┤                    │
 │           │ publish /tf dynamic                            │
 │           ├───────────────────────────────────────────────►│
 │ health?   │                                                │
 ├──────────►│ /tf/get_health_status                          │
 │◄──────────┤                                                │
```

### 6.3 与定位模块的职责约定

| 场景 | 发布策略 | 说明 |
|---|---|---|
| SLAM 自行发布 `map → odom` | `publish_map_to_odom=false` | TF 只发布 URDF 本体链，避免重复；Global TF Observer 会监测 SLAM 发布的 `map→odom` 并纳入 global chain 校验 |
| TF 统一发布基础链 | `publish_map_to_odom=true`，`base_pose_source=lidar_slam/vslam` | TF 将输入 pose 转为标准 frame 名；SLAM 不应再发布 `map→odom` |
| 多定位源并存 | `base_pose_source` 指定一个主源 | TF 不融合，只做主源选择；融合应在定位模块完成 |
| 无定位输入 | `publish_world_frames=false` | 只发布 `base_link` 以下机器人本体链；`base_footprint` 和世界链均不发布 |

---

## 7. 关键参数与配置
```yaml
# tf_publisher/config/tf_params.yaml

tf_publisher:
  ros__parameters:
    # URDF / robot model
    robot_description_param: "robot_description"  # URDF 参数名
    urdf_file: ""                                 # 可选：URDF 文件路径，非空时优先于参数
    robot_model_name: "humanoid"
    robot_root_frame: "base_link"                 # 【修订】原 root_frame → robot_root_frame；URDF 机器人本体树根
    world_root_frame: "map"                       # 【新增】全局 TF 树根；publish_world_frames=false 时不使用

    # frame naming
    frame_prefix: ""                              # 多机器人场景前缀，如 robot_1/
    required_frames:
      - "base_link"
      - "base_footprint"
      - "left_foot_link"
      - "right_foot_link"
      - "imu_link"
    allowed_duplicate_child_frames: []            # 默认不允许重复 child frame；白名单例外项（如 SLAM 也发布 odom→base_link 时填入 base_link）

    # publish behavior
    publish_rate_hz: 100.0                        # 动态 TF 发布频率（Hz）
    publish_static_tf: true
    publish_dynamic_joints: true
    publish_base_footprint: true                  # false 时退化为 odom→base_link，不发布 base_footprint
    publish_world_frames: true                    # false 时只发布 base_link 以下本体链
    publish_map_to_odom: false                    # true 时 TF 发布 map→odom；与 SLAM 自发布互斥
    publish_odom_to_base: true                    # true 时 TF 发布 odom→base_footprint→base_link

    # input source selection
    joint_state_source: "mc"                      # mc | joint_states | ethercat_hal
    base_pose_source: "lidar_slam"                # lidar_slam | vslam | mc | none（见 §5.4 输入语义矩阵）
    map_origin_source: "map_manager"              # map_manager | parameter

    # topic names（可覆盖默认值）
    mc_whole_body_state_topic: "/mc/whole_body_state"
    joint_states_topic: "/joint_states"
    ethercat_joint_states_topic: "/hal_ethercat/joint_states"
    vslam_pose_topic: "/vslam/pose"
    lidar_slam_pose_topic: "/lidar_slam/pose"
    map_origin_topic: "/map_manager/map_origin"

    # timing
    joint_state_timeout_ms: 100.0
    base_pose_timeout_ms: 200.0
    map_origin_timeout_ms: 1000.0
    max_timestamp_jump_ms: 500.0
    use_node_time_when_input_stamp_missing: false # true 时使用 node time 并上报 anomaly；E-Stop 冻结时此参数无效，始终保留原始时间戳

    # missing joint policy
    missing_joint_policy: "hold_last"             # strict | hold_last
                                                  # 【修订】移除 zero 策略（见 debug 参数组）
    hold_last_max_duration_ms: 500.0

    # QoS / monitor
    heartbeat_rate_hz: 1.0
    frame_graph_publish_rate_hz: 1.0
    min_dynamic_publish_rate_hz: 50.0
    enable_hds_health_report: true

    # Global TF Observer
    enable_global_tf_observer: true               # 【新增】启用全局 TF 冲突审计；关闭后 global_chain_complete 始终为 unknown
    global_observer_audit_rate_hz: 1.0            # 【新增】冲突审计周期

    # service behavior
    allow_runtime_reload: true
    reload_service_timeout_sec: 3.0

    # diagnostics
    log_frame_graph_on_startup: true
    anomaly_throttle_sec: 1.0

    # ── debug 参数组（仅 debug 构建可用，量产禁止设置）──
    # debug_missing_joint_policy_zero: false       # 启用后 missing_joint_policy 可设为 zero（关节跳零，仅用于离线调试）
```

---

## 8. 错误码定义

| 错误码 | 常量名 | 说明 | 严重级别 |
|---|---|---|---|
| 0 | `OK` | 成功 | — |
| 6001 | `ERR_TF_URDF_NOT_FOUND` | URDF 文件或 `robot_description` 参数不存在 | HIGH |
| 6002 | `ERR_TF_URDF_PARSE_FAILED` | URDF XML 解析失败或结构非法 | HIGH |
| 6003 | `ERR_TF_EMPTY_KINEMATIC_TREE` | URDF 中没有有效 link/joint | HIGH |
| 6004 | `ERR_TF_ROOT_FRAME_MISMATCH` | `robot_root_frame` 与 URDF 根 link 不一致 | MEDIUM |
| 6005 | `ERR_TF_FRAME_NAME_CONFLICT` | 同一 child frame 存在多个发布来源（含外部节点） | HIGH |
| 6006 | `ERR_TF_JOINT_STATE_TIMEOUT` | 关节状态输入超时 | MEDIUM |
| 6007 | `ERR_TF_BASE_POSE_TIMEOUT` | 基座/定位位姿输入超时 | HIGH |
| 6008 | `ERR_TF_MISSING_REQUIRED_FRAME` | 必需 frame 缺失 | HIGH |
| 6009 | `ERR_TF_TRANSFORM_TIMESTAMP` | transform 时间戳跳变、回退或过期 | MEDIUM |
| 6010 | `ERR_TF_RELOAD_REJECTED` | 热加载含禁止变更类型或校验失败 | MEDIUM |
| 6011 | `ERR_TF_STATIC_TF_PUBLISH_FAIL` | 静态 TF 发布失败 | HIGH |
| 6012 | `ERR_TF_DYNAMIC_TF_RATE_DROP` | 动态 TF 发布频率低于阈值 | MEDIUM |
| 6013 | `ERR_TF_UNSUPPORTED_JOINT_TYPE` | URDF 中含不支持的 joint 类型（floating/planar） | HIGH |

---

## 9. 安全约束

### 9.1 与运动安全的关系

- TF 不下发任何运动指令，不参与关节力矩/位置控制。
- TF 异常不得阻塞 MC 实时控制线程；MC 必须使用自身状态估计闭环，不依赖 TF 作为实时控制输入。
- TF 可被 PnC/Perception 用于坐标查询，因此 TF 断链或过期必须上报 HDS，由 HDS 决定是否影响导航/任务。

### 9.2 坐标树安全约束

1. **禁止重复 child frame 发布**：重复发布会造成感知/规划坐标跳变；由 Global TF Observer 在运行时检测外部节点冲突，检测到冲突后停止发布本节点对应 child frame 并上报 `ANOMALY_FRAME_CONFLICT`。
2. **禁止发布未校验 URDF**：URDF 解析失败、root frame 不一致或含不支持 joint 类型时进入 `TF_ERROR`。
3. **核心链路时间戳过期必须显式降级**：`odom → base_footprint → base_link` 或 `base_link` 以下主干链路超时时，发布 anomaly 和健康指标并进入 `TF_ERROR`。
4. **热加载失败不影响旧模型**：避免运行中坐标树被无效模型破坏；static child frame 不得通过热加载删除或重命名（见 §3.4）。
5. **E-Stop/FAULT 状态下继续发布最后可用 TF，但时间戳冻结**：
	- 保留最后一帧有效 TF 数据继续广播，便于诊断和数据记录。
	- **必须保留原始最后有效时间戳，禁止刷新为当前 node time**；否则下游会误判 TF 为新鲜数据。
	- `TfState.status_message` 必须标注 "input_frozen_since: "。
	- `use_node_time_when_input_stamp_missing` 参数在此场景下无效。

### 9.3 HDS 上报原则

TF 上报原始事实，不定级：

| 指标字段 | 说明 |
|---|---|
| `frame_tree_complete` | 本节点负责的所有 frame 是否全部正常发布 |
| `global_chain_complete` | 通过 Global TF Observer 观测到的全局链路是否完整 |
| `input_timeout_count` | 累计输入超时次数 |
| `max_transform_age_ms` | 当前最老 dynamic transform 的时间戳距今时长 |
| `duplicate_child_frames` | 检测到的重复 child frame 列表 |
| `missing_required_frames` | 当前缺失的 required frames 列表 |
| `dynamic_publish_rate_hz` | 最近 1s 内动态 TF 实际发布频率 |

HDS 根据这些指标和其他模块状态决定是否 WARNING/DEGRADED/FAULT。

---

## 10. 包结构
```plaintext
tf_msgs/
├── msg/
│   ├── TfState.msg
│   ├── FrameEdge.msg
│   ├── FrameGraph.msg
│   ├── TransformAnomaly.msg
│   ├── Heartbeat.msg
│   └── ErrorCode.msg
├── srv/
│   ├── ReloadRobotDescription.srv
│   ├── GetTfStatus.srv
│   ├── GetFrameGraph.srv
│   ├── ValidateFrameChain.srv
│   └── GetHealthStatus.srv
├── CMakeLists.txt
└── package.xml

tf_publisher/
├── include/tf_publisher/
│   ├── tf_publisher_node.hpp
│   ├── model_manager.hpp
│   ├── transform_builder.hpp
│   ├── input_adapters.hpp
│   ├── frame_graph_monitor.hpp
│   ├── global_tf_observer.hpp      # 【新增】
│   └── diagnostics_reporter.hpp
├── src/
│   ├── tf_publisher_node.cpp
│   ├── model_manager.cpp
│   ├── transform_builder.cpp
│   ├── input_adapters.cpp
│   ├── frame_graph_monitor.cpp
│   ├── global_tf_observer.cpp      # 【新增】
│   └── diagnostics_reporter.cpp
├── test/
│   ├── test_urdf_validation.cpp
│   ├── test_transform_builder.cpp
│   ├── test_frame_graph_monitor.cpp
│   ├── test_global_tf_observer.cpp # 【新增】
│   └── test_reload_robot_description.cpp
├── config/
│   └── tf_params.yaml
├── launch/
│   └── tf.launch.py
├── CMakeLists.txt
└── package.xml
```

---

## 11. 关键性能指标（KPI）

| 指标 | 目标值 | 说明 |
|---|---|---|
| 动态 TF 发布频率 | 默认 100Hz，最低 50Hz | 满足感知/规划坐标查询 |
| 静态 TF 首次发布时间 | < 500ms | 从节点启动到 `/tf_static` 可用 |
| URDF 解析时间 | < 300ms | 常规人形机器人模型（≤ 50 joints） |
| 热加载中断时间 | < 1 个发布周期（≤ 10ms @ 100Hz） | 原子替换，避免长时间断链 |
| 动态 transform 端到端延迟 | < 20ms | 从输入关节状态到 `/tf` 发布 |
| 坐标链校验服务响应 | < 20ms | 常规 frame chain 查询 |
| 心跳频率 | 1Hz | EM/HDS 监控 |
| 重复 frame 检出率 | 100% | 启动/热加载/运行期（通过 Global TF Observer）必须检出 |
| Global TF Observer 冲突检出延迟 | < 2s（1/audit_rate_hz × 2） | 外部节点发布冲突 frame 到 TF 上报 anomaly 的最大延迟 |

---
