# TF Publisher 模块设计

## 1. 模块概述与定位

**模块名称**：TF Publisher（TF）

**定位**：TF Publisher 是端侧软件系统中的**坐标变换发布与坐标树一致性维护模块**，位于 HAL & Infra 层。它读取机器人 URDF，构建机器人运动学树，订阅关节状态与定位位姿，统一发布 ROS2 标准 `/tf` 与 `/tf_static` 坐标变换，供感知、定位、规划、运动控制、数据记录和调试工具使用。

**核心职责**：
1. 读取并校验 URDF，生成机器人 link/joint 坐标树
2. 发布固定关节、传感器外参等静态坐标变换到 `/tf_static`
3. 根据关节状态发布活动关节动态坐标变换到 `/tf`
4. 按配置转发/发布 `map → odom → base_link` 等基础坐标链
5. 监控坐标树完整性、时间戳新鲜度和 frame 命名冲突
6. 发布心跳与原始健康指标，供 EM/HDS 监控

**与相邻模块的边界**：

| 边界 | TF 负责 | 对方负责 |
|------|---------|---------|
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

```
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
|------|------|---------|
| 上层 | Perception / PnC / DR / 调试工具 | 查询坐标变换、做感知融合/规划/记录 |
| **本模块** | **TF Publisher** | **读取 URDF，发布标准 TF 坐标树，监控坐标树一致性** |
| 下层/输入 | MC / VSLAM / Lidar-SLAM / MapManager | 提供关节状态、基座/地图位姿、地图 frame 配置 |

### 2.1 坐标系约定

| Frame | 父 Frame | 来源 | 说明 |
|-------|----------|------|------|
| `map` | 无 | MapManager / SLAM | 全局地图坐标系 |
| `odom` | `map` | VSLAM/Lidar-SLAM 或外部定位桥 | 局部连续里程计坐标系 |
| `base_link` | `odom` | MC 或 SLAM 位姿 | 机器人基座坐标系 |
| `base_footprint` | `odom` | TF 投影计算 | `base_link` 在地面的 yaw-only 投影 |
| `torso_link` / `pelvis_link` | `base_link` | URDF + 关节状态 | 躯干/骨盆 link |
| `left_foot_link` / `right_foot_link` | URDF 父 link | URDF + 关节状态 | 足端坐标系 |
| `camera_*_frame` | URDF 父 link | URDF fixed joint | 相机外参坐标系 |
| `lidar_*_frame` | URDF 父 link | URDF fixed joint | 雷达外参坐标系 |
| `imu_link` | URDF 父 link | URDF fixed joint | IMU 安装坐标系 |

### 2.2 坐标发布原则

1. **URDF 是机器人本体 frame 权威来源**：link/joint 名称、父子关系、固定外参均来自 URDF。
2. **关节状态是动态关节角权威来源**：默认使用 `/mc/whole_body_state`，可配置使用标准 `/joint_states` 或 `/hal_ethercat/joint_states`。
3. **定位模块是全局位姿权威来源**：TF 只按配置转发或重命名定位结果，不融合定位。
4. **同一 child frame 只能有一个发布者**：启动时检查 TF 发布白名单，避免与 SLAM、robot_state_publisher 或其他节点重复发布。
5. **静态与动态分离**：fixed joint 和传感器外参只发布到 `/tf_static`；活动关节和基座位姿发布到 `/tf`。

---

## 3. 状态机设计

### 3.1 TF 内部运行状态

| 状态 | 值 | 说明 |
|------|-----|------|
| `TF_INIT` | 0 | 节点初始化中，参数尚未加载完成 |
| `TF_LOADING_MODEL` | 1 | 正在读取并解析 URDF |
| `TF_WAITING_INPUT` | 2 | URDF 已加载，等待关节状态或定位输入 |
| `TF_PUBLISHING` | 3 | 正常发布 `/tf` 与 `/tf_static` |
| `TF_RELOADING` | 4 | 正在热加载 URDF/参数 |
| `TF_DEGRADED` | 5 | 部分输入缺失或坐标链不完整，但可发布可用部分 |
| `TF_ERROR` | 6 | URDF 无效、frame 冲突或核心坐标链不可用 |

### 3.2 状态转换图

```
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
                               │ reload_failed                │ input_timeout /
                               │                              │ chain_broken
                         ┌─────┴──────┐ reload_request ┌──────▼──────┐
                         │TF_RELOADING│◄───────────────│ TF_DEGRADED │
                         └─────┬──────┘                └──────┬──────┘
                               │ reload_ok                    │ recovered
                               └──────────────────────────────►│
                                                              ▼
                                                       TF_PUBLISHING
```

### 3.3 状态转换约束

1. **URDF 解析失败必须进入 `TF_ERROR`**，不允许发布不完整或猜测得到的坐标树。
2. **核心坐标链缺失进入 `TF_ERROR`**：`map/odom/base_link` 发布启用时，任一核心 frame 配置冲突或缺失即错误。
3. **非核心输入超时进入 `TF_DEGRADED`**：例如某个手臂关节状态过期，可继续发布其他可用 link，并上报 HDS 原始健康数据。
4. **热加载必须原子切换**：新 URDF 校验通过后才替换运行中的 kinematic tree；失败时保持旧模型。
5. **TF 不触发 SM 状态转换**：严重错误只通过心跳/健康上报给 HDS/EM，由 HDS/EM 决策。

---

## 4. ROS2 接口定义

### 4.1 消息定义（msg）

```
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
string root_frame
uint32 frame_count
uint32 dynamic_joint_count
uint32 static_transform_count
uint32 published_transform_count
float32 max_transform_age_ms
bool frame_tree_complete
string status_message
builtin_interfaces/Time stamp
```

```
# tf_msgs/msg/FrameEdge.msg
# 坐标树中的一条父子边

string parent_frame
string child_frame
string joint_name
uint8 joint_type              # 0=fixed, 1=revolute, 2=continuous, 3=prismatic, 4=floating, 5=planar
bool is_static
bool is_published
float32 last_update_age_ms
string source                 # urdf | mc | joint_states | vslam | lidar_slam | map_manager
```

```
# tf_msgs/msg/FrameGraph.msg
# TF 坐标树摘要，用于监控和调试

string root_frame
string[] frame_names
tf_msgs/FrameEdge[] edges
string[] missing_frames
string[] duplicate_child_frames
uint32 disconnected_subtree_count
bool is_tree_valid
string validation_message
builtin_interfaces/Time stamp
```

```
# tf_msgs/msg/TransformAnomaly.msg
# TF 原始异常事件，不做故障定级

uint8 ANOMALY_URDF_INVALID       = 1
uint8 ANOMALY_FRAME_CONFLICT     = 2
uint8 ANOMALY_INPUT_TIMEOUT      = 3
uint8 ANOMALY_CHAIN_DISCONNECTED = 4
uint8 ANOMALY_TIMESTAMP_JUMP     = 5
uint8 ANOMALY_JOINT_MISSING      = 6
uint8 ANOMALY_RATE_DROP          = 7

uint8 anomaly_type
string frame_id
string child_frame_id
string source
float32 observed_value
float32 threshold_value
string message
builtin_interfaces/Time stamp
```

```
# tf_msgs/msg/Heartbeat.msg
# TF 模块心跳

builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message
bool urdf_loaded
bool frame_tree_complete
uint32 published_transform_count
uint32 input_timeout_count
```

```
# tf_msgs/msg/ErrorCode.msg
# TF 错误码定义（原名 TfErrorCode.msg，应改为 ErrorCode.msg）

uint16 OK                          = 0
uint16 ERR_TF_URDF_NOT_FOUND          = 6001   # URDF 文件或 robot_description 参数不存在
uint16 ERR_TF_URDF_PARSE_FAILED       = 6002   # URDF 解析失败
uint16 ERR_TF_EMPTY_KINEMATIC_TREE    = 6003   # URDF 中没有有效 link/joint
uint16 ERR_TF_ROOT_FRAME_MISMATCH     = 6004   # URDF 根 frame 与配置不一致
uint16 ERR_TF_FRAME_NAME_CONFLICT     = 6005   # child frame 被多个来源发布
uint16 ERR_TF_JOINT_STATE_TIMEOUT     = 6006   # 关节状态输入超时
uint16 ERR_TF_BASE_POSE_TIMEOUT       = 6007   # 基座/定位位姿输入超时
uint16 ERR_TF_MISSING_REQUIRED_FRAME  = 6008   # 必需 frame 缺失
uint16 ERR_TF_TRANSFORM_TIMESTAMP     = 6009   # 时间戳跳变或过期
uint16 ERR_TF_RELOAD_REJECTED         = 6010   # 热加载校验失败
uint16 ERR_TF_STATIC_TF_PUBLISH_FAIL  = 6011   # 静态 TF 发布失败
uint16 ERR_TF_DYNAMIC_TF_RATE_DROP    = 6012   # 动态 TF 发布频率低于阈值

uint16 error_code
string message
```

### 4.2 服务定义（srv）

```
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
```

```
# tf_msgs/srv/GetTfStatus.srv
# 查询 TF 模块状态

# Request（空）
---
bool success
string message
tf_msgs/TfState state
tf_msgs/FrameGraph frame_graph
```

```
# tf_msgs/srv/GetFrameGraph.srv
# 获取坐标树摘要

bool include_static
bool include_dynamic
---
bool success
string message
tf_msgs/FrameGraph frame_graph
```

```
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

```
# tf_msgs/srv/GetHealthStatus.srv
# 健康状态查询，供 EM/HDS 调用

---
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
uint32 frame_count
uint32 input_timeout_count
```

### 4.3 Action 定义

TF Publisher 不提供 Action。URDF 热加载和坐标链校验均为短耗时同步操作，使用 Service 即可；长期运行状态通过 Topic 广播。

### 4.4 接口汇总表

#### Topics（发布）

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/tf` | `tf2_msgs/msg/TFMessage` | TF → ALL | Best Effort, Volatile, Depth 100 | 50-200Hz | 动态坐标变换 |
| `/tf_static` | `tf2_msgs/msg/TFMessage` | TF → ALL | Reliable + Transient Local, Depth 1 | 启动/重载时 | 静态坐标变换 |
| `/tf/tf_state` | `tf_msgs/msg/TfState` | TF → ALL | Reliable + Volatile, Depth 1 | 1Hz | TF 模块状态 |
| `/tf/frame_graph` | `tf_msgs/msg/FrameGraph` | TF → DR/UI/HDS | Reliable + Transient Local, Depth 1 | 1Hz / 事件驱动 | 坐标树摘要 |
| `/tf/transform_anomaly` | `tf_msgs/msg/TransformAnomaly` | TF → HDS/DR | Reliable + Volatile, Depth 20 | 事件驱动 | TF 原始异常 |
| `/tf/heartbeat` | `tf_msgs/msg/Heartbeat` | TF → EM/HDS | Reliable + Volatile, Depth 1 | 1Hz | 心跳 |
| `/hds/health_report` | `hds_msgs/msg/HealthReport` | TF → HDS | Reliable + Volatile, Depth 10 | 1Hz / 事件驱动 | 原始健康指标上报 |

#### Topics（订阅）

| 名称 | 类型 | 来源 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/mc/whole_body_state` | `mc_msgs/msg/WholeBodyState` | MC | Best Effort, Depth 1 | 100-1000Hz | 默认关节状态与基座姿态来源 |
| `/joint_states` | `sensor_msgs/msg/JointState` | 可选标准关节状态源 | Best Effort, Depth 10 | 50-1000Hz | 兼容标准 ROS 工具链 |
| `/hal_ethercat/joint_states` | `hal_ethercat_msgs/msg/JointState` | HAL_EtherCAT | Best Effort, Depth 1 | 1kHz | 可选低层关节原始状态源 |
| `/vslam/pose` | `geometry_msgs/msg/PoseWithCovarianceStamped` | VSLAM | Best Effort, Depth 1 | 30Hz | 可选视觉定位位姿 |
| `/lidar_slam/pose` | `geometry_msgs/msg/PoseWithCovarianceStamped` | Lidar-SLAM | Best Effort, Depth 1 | 20Hz | 可选激光定位位姿 |
| `/map_manager/map_origin` | `geometry_msgs/msg/PoseStamped` | MapManager | Reliable + Transient Local, Depth 1 | 事件驱动 | 地图原点/地图 frame 配置 |
| `/sm/robot_state` | `sm_msgs/msg/RobotState` | SM | Reliable + Transient Local, Depth 1 | 事件驱动 | 用于停机/故障时标记状态，不参与运动决策 |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/tf/reload_robot_description` | `tf_msgs/srv/ReloadRobotDescription` | EM / 调试工具 | 热加载或校验 URDF |
| `/tf/get_tf_status` | `tf_msgs/srv/GetTfStatus` | EM / HDS / 调试工具 | 查询 TF 状态 |
| `/tf/get_frame_graph` | `tf_msgs/srv/GetFrameGraph` | DR / UI / 调试工具 | 获取坐标树摘要 |
| `/tf/validate_frame_chain` | `tf_msgs/srv/ValidateFrameChain` | Perception / PnC / 调试工具 | 校验 frame 链可达性 |
| `/tf/get_health_status` | `tf_msgs/srv/GetHealthStatus` | EM / HDS | 健康查询 |

#### Actions

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| 无 | 无 | 无 | TF 不提供 Action |

---

## 5. 内部设计

### 5.1 节点架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                      CodexTfPublisherNode                           │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                  Model Manager                              │   │
│  │  - 读取 robot_description / URDF 文件                        │   │
│  │  - 解析 link/joint/tree                                      │   │
│  │  - 校验 root_frame、重复 child frame、joint 限位              │   │
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
│  │  - base pose → odom/base_link transform                      │   │
│  │  - base_link → base_footprint projection                     │   │
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
│  │  - 坐标树完整性校验                                          │   │
│  │  - 输入超时/时间跳变检测                                     │   │
│  │  - 异常事件与健康指标发布                                    │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  ROS2 Callback Groups:                                              │
│  - cb_group_input: 高频输入订阅                                     │
│  - cb_group_publish: 定时 TF 发布                                   │
│  - cb_group_service: reload/status/validate 服务                    │
└─────────────────────────────────────────────────────────────────────┘
```

### 5.2 关键设计决策

1. **优先复用 ROS2 标准 TF 生态**：对外发布标准 `tf2_msgs/msg/TFMessage`，业务模块通过 tf2 buffer/listener 查询，不引入私有坐标查询协议。
2. **TF 发布与运动控制解耦**：TF 发布频率低于 MC 1kHz 控制环，默认 100Hz；TF 丢帧不得阻塞 MC。
3. **URDF 原子热加载**：热加载先在影子模型中解析校验，通过后一次性替换；失败时继续使用旧模型并返回 `ERR_RELOAD_REJECTED`。
4. **输入源可配置但同类只允许一个权威源**：例如关节状态只能选择 `mc`、`joint_states`、`ethercat_hal` 之一作为主源，避免同一 child frame 多发布。
5. **时间戳策略显式化**：动态 TF 默认使用输入消息时间戳；当输入时间戳缺失或过期时，可配置使用 node time，但必须上报 anomaly。
6. **坐标树监控只输出原始事实**：TF 只报告断链、超时、冲突等，不决定系统进入 `DEGRADED` 或 `FAULT`。

### 5.3 关键流程

#### 5.3.1 启动流程

```
EM 启动 TF 进程
  → 声明并读取参数
  → 状态切换 TF_LOADING_MODEL
  → 从 robot_description 参数或 URDF 文件读取模型
  → 解析 URDF：
      1. 校验 link/joint 非空
      2. 校验 root_frame
      3. 校验 frame 命名唯一
      4. 拆分 fixed joint 与 movable joint
  → 发布 /tf_static（fixed joint、传感器外参）
  → 订阅配置的关节状态源、定位源、SM 状态
  → 状态切换 TF_WAITING_INPUT
  → 收到必要输入后进入 TF_PUBLISHING
  → 周期发布 /tf/heartbeat、/tf/tf_state、/tf/frame_graph
```

#### 5.3.2 动态 TF 发布流程

```
publish_timer 每 10ms（默认 100Hz）执行：
  1. 读取最新关节状态缓存
  2. 检查输入时间戳是否超过 joint_state_timeout_ms
  3. 遍历 DynamicJointMap：
      - 从关节状态中查找 joint position
      - 按 URDF joint origin + axis 计算 parent → child transform
      - 缺失关节按策略处理：
          strict: 不发布该 subtree，进入 TF_DEGRADED
          hold_last: 使用上一次合法值，并上报 anomaly
  4. 读取定位/基座位姿缓存（如启用）
  5. 生成 map/odom/base_link/base_footprint 变换
  6. 批量发布 tf2_msgs/TFMessage 到 /tf
  7. 更新 FrameGraphMonitor 指标
```

#### 5.3.3 URDF 热加载流程

```
EM/调试工具调用 /tf/reload_robot_description
  → Service 回调进入 TF_RELOADING
  → 读取新 URDF 或 robot_description
  → 在影子模型中解析并完整校验
  → dry_run=true：
      - 返回校验结果，不替换当前模型
  → dry_run=false 且校验通过：
      - 暂停 publish_timer 一个周期
      - 原子替换 kinematic tree 与 frame graph
      - 重新发布 /tf_static
      - 清空旧 joint cache 中不存在的关节
      - 恢复 TF_PUBLISHING 或 TF_WAITING_INPUT
  → 校验失败：
      - 保持旧模型
      - 发布 TransformAnomaly
      - 返回 success=false
```

#### 5.3.4 坐标树异常处理流程

```
FrameGraphMonitor 周期检查：
  - required_frames 是否全部存在？
  - child_frame 是否被重复发布？
  - 动态 transform age 是否超过阈值？
  - /tf 发布频率是否低于 min_publish_rate_hz？
  - 时间戳是否发生大幅回跳？

异常出现：
  → 发布 /tf/transform_anomaly
  → 更新 /tf/tf_state.status_message
  → 发布 /hds/health_report 原始指标
  → 非核心异常：TF_DEGRADED
  → 核心链路异常：TF_ERROR
```

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|------|------|------|------|
| MC | MC → TF | `/mc/whole_body_state` | 默认关节状态、基座姿态输入 |
| HAL_EtherCAT | HAL_EtherCAT → TF | `/hal_ethercat/joint_states` | 可选关节原始状态输入 |
| VSLAM | VSLAM → TF | `/vslam/pose` | 可选视觉定位输入 |
| Lidar-SLAM | Lidar-SLAM → TF | `/lidar_slam/pose` | 可选激光定位输入 |
| MapManager | MapManager → TF | `/map_manager/map_origin` | 地图 frame/原点配置 |
| SM | SM → TF | `/sm/robot_state` | 运行状态上下文，用于健康标记 |
| Perception | TF → Perception | `/tf`, `/tf_static` | 传感器到机体/地图坐标变换 |
| PnC | TF → PnC | `/tf`, `/tf_static` | 规划坐标链查询 |
| DR | TF → DR | `/tf`, `/tf_static`, `/tf/frame_graph` | 数据记录与离线回放 |
| HDS | TF → HDS | `/tf/heartbeat`, `/tf/transform_anomaly`, `/hds/health_report` | 心跳与原始健康指标 |
| EM | EM → TF | 进程启停、`/tf/get_health_status` | 生命周期管理和健康查询 |
| 调试工具 | 调试工具 → TF | `/tf/reload_robot_description`, `/tf/get_frame_graph` | 模型热加载与坐标树调试 |

### 6.2 启动时序

```
 EM          TF          MC        SLAM/MapManager      Perception/PnC
 │           │           │              │                    │
 │ start TF  │           │              │                    │
 ├──────────►│           │              │                    │
 │           │ load URDF │              │                    │
 │           ├───────────┤              │                    │
 │           │ publish /tf_static       │                    │
 │           ├───────────────────────────────────────────────►│
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
|------|----------|------|
| SLAM 自行发布 `map → odom` | `publish_map_to_odom=false` | TF 只发布 URDF 本体链，避免重复 |
| TF 统一发布基础链 | `publish_map_to_odom=true`，`base_pose_source=lidar_slam/vslam` | TF 将输入 pose 转为标准 frame 名 |
| 多定位源并存 | `base_pose_source` 指定一个主源 | TF 不融合，只做主源选择；融合应在定位模块完成 |
| 无定位输入 | `publish_world_frames=false` | 只发布 `base_link` 以下机器人本体链 |

---

## 7. 关键参数与配置

```yaml
# tf_publisher/config/tf_params.yaml

tf_publisher:
  ros__parameters:
    # URDF / robot model
    robot_description_param: "robot_description"  # URDF 参数名
    urdf_file: ""                                 # 可选：URDF 文件路径，非空时优先
    robot_model_name: "humanoid"
    root_frame: "base_link"

    # frame naming
    frame_prefix: ""                              # 多机器人场景前缀，如 robot_1/
    required_frames:
      - "base_link"
      - "left_foot_link"
      - "right_foot_link"
      - "imu_link"
    allowed_duplicate_child_frames: []            # 默认不允许重复 child frame

    # publish behavior
    publish_rate_hz: 100.0                        # 动态 TF 发布频率
    publish_static_tf: true
    publish_dynamic_joints: true
    publish_base_footprint: true
    publish_world_frames: true
    publish_map_to_odom: false                    # true 时 TF 发布 map→odom
    publish_odom_to_base: true                    # true 时 TF 发布 odom→base_link

    # input source selection
    joint_state_source: "mc"                       # mc | joint_states | ethercat_hal
    base_pose_source: "lidar_slam"                 # lidar_slam | vslam | mc | none
    map_origin_source: "map_manager"               # map_manager | parameter

    # topic names
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
    use_node_time_when_input_stamp_missing: false

    # missing joint policy
    missing_joint_policy: "hold_last"              # strict | hold_last | zero
    hold_last_max_duration_ms: 500.0

    # QoS / monitor
    heartbeat_rate_hz: 1.0
    frame_graph_publish_rate_hz: 1.0
    min_dynamic_publish_rate_hz: 50.0
    enable_hds_health_report: true

    # service behavior
    allow_runtime_reload: true
    reload_service_timeout_sec: 3.0

    # diagnostics
    log_frame_graph_on_startup: true
    anomaly_throttle_sec: 1.0
```

---

## 8. 错误码定义

| 错误码 | 常量名 | 说明 | 严重级别 |
|--------|--------|------|---------|
| 0 | `OK` | 成功 | - |
| 6001 | `ERR_TF_URDF_NOT_FOUND` | URDF 文件或 `robot_description` 参数不存在 | HIGH |
| 6002 | `ERR_TF_URDF_PARSE_FAILED` | URDF XML 解析失败或结构非法 | HIGH |
| 6003 | `ERR_TF_EMPTY_KINEMATIC_TREE` | URDF 中没有有效 link/joint | HIGH |
| 6004 | `ERR_TF_ROOT_FRAME_MISMATCH` | 根 frame 与配置不一致 | MEDIUM |
| 6005 | `ERR_TF_FRAME_NAME_CONFLICT` | 同一 child frame 存在多个发布来源 | HIGH |
| 6006 | `ERR_TF_JOINT_STATE_TIMEOUT` | 关节状态输入超时 | MEDIUM |
| 6007 | `ERR_TF_BASE_POSE_TIMEOUT` | 基座/定位位姿输入超时 | HIGH |
| 6008 | `ERR_TF_MISSING_REQUIRED_FRAME` | 必需 frame 缺失 | HIGH |
| 6009 | `ERR_TF_TRANSFORM_TIMESTAMP` | transform 时间戳跳变、回退或过期 | MEDIUM |
| 6010 | `ERR_TF_RELOAD_REJECTED` | 热加载校验失败，已拒绝切换 | MEDIUM |
| 6011 | `ERR_TF_STATIC_TF_PUBLISH_FAIL` | 静态 TF 发布失败 | HIGH |
| 6012 | `ERR_TF_DYNAMIC_TF_RATE_DROP` | 动态 TF 发布频率低于阈值 | MEDIUM |

---

## 9. 安全约束

### 9.1 与运动安全的关系

- TF 不下发任何运动指令，不参与关节力矩/位置控制。
- TF 异常不得阻塞 MC 实时控制线程；MC 必须使用自身状态估计闭环，不依赖 TF 作为实时控制输入。
- TF 可被 PnC/Perception 用于坐标查询，因此 TF 断链或过期必须上报 HDS，由 HDS 决定是否影响导航/任务。

### 9.2 坐标树安全约束

1. **禁止重复 child frame 发布**：重复发布会造成感知/规划坐标跳变，启动校验必须拦截。
2. **禁止发布未校验 URDF**：URDF 解析失败或 root frame 不一致时进入 `TF_ERROR`。
3. **核心链路时间戳过期必须显式降级**：`odom → base_link` 或 `base_link` 以下主干链路超时时，发布 anomaly 和健康指标。
4. **热加载失败不影响旧模型**：避免运行中坐标树被无效模型破坏。
5. **E-Stop/FAULT 状态下继续发布最后可用 TF 状态**：便于诊断和数据记录；但状态中必须标记输入是否过期。

### 9.3 HDS 上报原则

TF 上报原始事实，不定级：

- `frame_tree_complete=false`
- `input_timeout_count`
- `max_transform_age_ms`
- `duplicate_child_frames`
- `missing_required_frames`
- `dynamic_publish_rate_hz`

HDS 根据这些指标和其他模块状态决定是否 WARNING/DEGRADED/FAULT。

---

## 10. 包结构

```
tf_msgs/
├── msg/
│   ├── TfState.msg
│   ├── FrameEdge.msg
│   ├── FrameGraph.msg
│   ├── TransformAnomaly.msg
│   ├── Heartbeat.msg
│   └── ErrorCode.msg               # 错误码（原名 TfErrorCode.msg）
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
│   └── diagnostics_reporter.hpp
├── src/
│   ├── tf_publisher_node.cpp
│   ├── model_manager.cpp
│   ├── transform_builder.cpp
│   ├── input_adapters.cpp
│   ├── frame_graph_monitor.cpp
│   └── diagnostics_reporter.cpp
├── test/
│   ├── test_urdf_validation.cpp
│   ├── test_transform_builder.cpp
│   ├── test_frame_graph_monitor.cpp
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
|------|--------|------|
| 动态 TF 发布频率 | 默认 100Hz，最低 50Hz | 满足感知/规划坐标查询 |
| 静态 TF 首次发布时间 | < 500ms | 从节点启动到 `/tf_static` 可用 |
| URDF 解析时间 | < 300ms | 常规机器人模型 |
| 热加载中断时间 | < 1 个发布周期 | 原子替换，避免长时间断链 |
| 动态 transform 端到端延迟 | < 20ms | 从输入关节状态到 `/tf` 发布 |
| 坐标链校验服务响应 | < 20ms | 常规 frame chain 查询 |
| 心跳频率 | 1Hz | EM/HDS 监控 |
| 重复 frame 检出率 | 100% | 启动/热加载阶段必须检出 |
