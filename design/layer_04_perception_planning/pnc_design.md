---

# Planning and Control 模块设计

## 1. 模块概述与定位

**模块名称**：Planning and Control（PnC）

**定位**：PnC 是端侧软件系统中**自主导航与运动规划中枢**，位于感知/规划层。它负责接收高层导航目标（来自 TE/Agent），融合感知与地图信息，生成安全可行的行走路径，并输出连续的控制信号（前后速度、转角速度）给 MC，驱动足式人形机器人自主行走。PnC 是连接"高层意图"与"底层行走"的桥梁，必须在动态环境中实时响应障碍物变化。

**核心职责**：
1. **全局路径规划**：基于地图生成从起点到终点的无碰撞全局路径
2. **局部路径规划/轨迹跟踪**：在全局路径引导下，结合实时感知生成局部轨迹，跟踪行走
3. **行走控制信号生成**：输出线速度（vx, vy）和角速度（yaw_rate）给 MC
4. **地形适应性评估**：评估地形坡度、粗糙度、可通行性，调整 foothold 策略
5. **动态避障**：实时检测障碍物，调整局部路径避免碰撞
6. **Foothold 规划**（可选）：在复杂地形下规划落脚点序列

**与相邻模块的边界**：

| 边界 | PnC 负责 | 对方负责 |
|------|---------|---------|
| PnC ↔ MC | 输出行走控制信号（vx, vy, yaw_rate） | 全身运动控制、RL 策略、关节力矩输出 |
| PnC ↔ Perception | 接收障碍物、地形、可通行区域信息 | 视觉+Lidar 感知融合 |
| PnC ↔ VSLAM | 接收视觉定位（机器人在地图中的位姿） | 视觉 SLAM 建图定位 |
| PnC ↔ Lidar-SLAM | 接收激光定位（位姿+点云地图） | 激光 SLAM 建图定位 |
| PnC ↔ MapManager | 查询/接收全局地图、地形高程图 | 地图管理、地图更新 |
| PnC ↔ TE | 接收导航任务目标（通过 Action） | 任务调度、生命周期管理 |
| PnC ↔ SM | 订阅状态缓存、导航前校验 | 全局状态机决策 |
| PnC ↔ HDS | 上报导航异常、规划失败等原始数据 | 故障诊断与定级 |

---

## 2. 状态机设计

### 2.1 PnC 内部运行状态

| 状态 | 值 | 说明 |
|------|-----|------|
| `PNC_IDLE` | 0 | 空闲，无导航任务 |
| `PNC_PLANNING` | 1 | 正在规划全局路径 |
| `PNC_NAVIGATING` | 2 | 正在执行导航，跟踪全局路径 |
| `PNC_AVOIDING` | 3 | 正在动态避障，局部重规划 |
| `PNC_REACHED` | 4 | 到达目标点 |
| `PNC_FAILED` | 5 | 规划失败或导航失败 |
| `PNC_PAUSED` | 6 | 导航暂停（用户暂停或任务暂停）|

### 2.2 状态转换图

```
                              ┌─────────────────────────────────────┐
                              │                                     │
                        ┌─────┴─────┐  goal_received    ┌───────────┴───┐
         startup───────►│  PNC_IDLE │──────────────────►│  PNC_PLANNING │
                        └───────────┘                   └──────┬────────┘
                              ▲                                │
                              │                                │ plan_success
                              │                                ▼
                              │                          ┌─────────────┐
                              │                          │PNC_NAVIGATING│
                              │                          └──────┬──────┘
                              │                                 │
                              │         goal_reached            │
                              │◄────────────────────────────────┤
                              │                                 │
                              │    obstacle_detected            │
                              │◄────────────────────────────────┤
                              │                                 │
                              │         plan_failed             │
                              │◄────────────────────────────────┤
                              │                                 │
                        ┌─────┴─────┐                     ┌─────┴─────┐
                        │PNC_REACHED│                     │PNC_FAILED │
                        └───────────┘                     └───────────┘

    PNC_NAVIGATING ──obstacle_detected──► PNC_AVOIDING ──avoid_success──► PNC_NAVIGATING
    PNC_NAVIGATING ──pause_request──► PNC_PAUSED ──resume_request──► PNC_NAVIGATING
    任意状态 ──cancel_request──► PNC_IDLE
```

### 2.3 状态转换约束

1. **只有 SM 的 ACTIVE_WALKING / ACTIVE_MOTION 状态下才允许进入 PNC_NAVIGATING** — PnC 在 ROS2 回调中检查 SM 状态，非法状态下拒绝导航请求
2. **全局路径规划失败不允许自动重试超过 3 次** — 避免无限循环消耗计算资源
3. **动态避障连续失败 3 次 → 进入 PNC_FAILED** — 上报 TE 决策是否重新规划或人工接管
4. **到达目标点的判定阈值**：位置误差 < 0.3m，姿态误差 < 0.2rad

---

## 3. ROS2 接口定义

### 3.1 消息定义（msg）

```
# pnc_msgs/msg/PncState.msg
# PnC 模块状态

uint8 PNC_IDLE       = 0
uint8 PNC_PLANNING   = 1
uint8 PNC_NAVIGATING = 2
uint8 PNC_AVOIDING   = 3
uint8 PNC_REACHED    = 4
uint8 PNC_FAILED     = 5
uint8 PNC_PAUSED     = 6

uint8 state                    # 当前状态
uint8 previous_state           # 上一个状态

# 导航目标信息
geometry_msgs/PoseStamped goal_pose      # 目标位姿
float32 goal_distance_m        # 到目标的直线距离 [m]
float32 path_progress_percent  # 路径完成度 0.0~100.0

# 当前控制输出
geometry_msgs/Twist current_cmd          # 当前行走控制信号

# 规划信息
float32 planning_time_ms       # 最近一次规划耗时 [ms]
uint32 replan_count            # 重规划次数

builtin_interfaces/Time stamp
```

```
# pnc_msgs/msg/PathInfo.msg
# 路径信息（用于监控和调试）

nav_msgs/Path global_path      # 全局路径
nav_msgs/Path local_path       # 局部路径
float64 path_length_m          # 路径总长度 [m]
float64 estimated_time_sec     # 预计到达时间 [s]
bool is_path_valid             # 路径是否有效（无碰撞）
string invalid_reason          # 路径无效原因

builtin_interfaces/Time stamp
```

```
# pnc_msgs/msg/TerrainCostmap.msg
# 地形代价地图（PnC 生成或接收）

nav_msgs/OccupancyGrid costmap           # 代价地图
float64 resolution             # 分辨率 [m/cell]
float64 origin_x               # 原点 X [m]
float64 origin_y               # 原点 Y [m]
string[] cost_layers           # 代价层名称（坡度、粗糙度、障碍物等）

builtin_interfaces/Time stamp
```

```
# pnc_msgs/msg/ErrorCode.msg
# PnC 错误码（按 interface_standards.md 统一命名，禁止前缀式命名）

uint16 OK                          = 0
uint16 ERR_INVALID_GOAL            = 4001   # 目标点无效（在障碍物内/不可达）
uint16 ERR_GLOBAL_PLAN_FAILED      = 4002   # 全局路径规划失败
uint16 ERR_LOCAL_PLAN_FAILED       = 4003   # 局部路径规划失败
uint16 ERR_OBSTACLE_BLOCKED        = 4004   # 被障碍物完全阻挡
uint16 ERR_LOCALIZATION_LOST       = 4005   # 定位丢失
uint16 ERR_MAP_UNAVAILABLE         = 4006   # 地图不可用
uint16 ERR_NAVIGATION_TIMEOUT      = 4007   # 导航超时
uint16 ERR_MOTION_NOT_ALLOWED      = 4008   # SM 状态不允许导航
uint16 ERR_PATH_DEVIATION          = 4009   # 路径偏离过大
uint16 ERR_REPLAN_EXHAUSTED        = 4010   # 重规划次数耗尽

uint16 error_code
string message
```

```
# pnc_msgs/msg/Heartbeat.msg
# PnC 心跳

builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message
bool planner_healthy
bool controller_healthy
```

### 3.2 服务定义（srv）

```
# pnc_msgs/srv/SetGoal.srv
# 设置导航目标（通常由 TE 调用）

geometry_msgs/PoseStamped goal_pose
string task_id
float64 timeout_sec              # 导航超时时间
---
bool success
uint16 error_code
string message
float64 estimated_time_sec
```

```
# pnc_msgs/srv/GetPncState.srv
# 查询 PnC 状态

# Request（空）
---
pnc_msgs/PncState state
pnc_msgs/PathInfo path_info
```

```
# pnc_msgs/srv/CancelNavigation.srv
# 取消当前导航

# Request（空）
---
bool success
string message
```

```
# pnc_msgs/srv/GetHealthStatus.srv
# 健康状态查询

# Request（空）
---
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
```

### 3.3 Action 定义（action）

```
# pnc_msgs/action/NavigateTo.action
# 导航到目标点（TE 调用）

# Goal
geometry_msgs/PoseStamped target_pose
string task_id
float64 timeout_sec
bool use_terrain_adaptation    # 是否启用地形适应
---
# Result
bool success
uint16 error_code
string message
geometry_msgs/PoseStamped final_pose
float64 total_distance_m
float64 total_time_sec
---
# Feedback
float32 progress_percent       # 0.0~100.0
float32 remaining_distance_m
geometry_msgs/Twist current_velocity_cmd
string current_state           # 当前 PnC 状态名
bool is_avoiding               # 是否正在避障
```

### 3.4 接口汇总表

#### Topics（发布）

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/pnc/velocity_command` | `geometry_msgs/msg/Twist` | PnC → MC | Best Effort, Depth 1 | 100Hz | 行走控制信号 |
| `/pnc/pnc_state` | `pnc_msgs/msg/PncState` | PnC → ALL | Reliable + Volatile, Depth 1 | 10Hz | PnC 状态广播 |
| `/pnc/path_info` | `pnc_msgs/msg/PathInfo` | PnC → DR/UI | Reliable + Volatile, Depth 1 | 1Hz | 路径信息（调试用）|
| `/pnc/terrain_costmap` | `pnc_msgs/msg/TerrainCostmap` | PnC → DR | Reliable + Volatile, Depth 1 | 1Hz | 地形代价地图 |
| `/pnc/heartbeat` | `pnc_msgs/msg/Heartbeat` | PnC → EM/HDS | Reliable + Volatile, Depth 1 | 1Hz | 心跳 |

#### Topics（订阅）

| 名称 | 类型 | 来源 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/sm/robot_state` | `sm_msgs/msg/RobotState` | SM | Reliable + Transient Local | 事件驱动 | 全局状态校验 |
| `/perception/obstacles` | `perception_msgs/msg/ObstacleArray` | Perception | Best Effort, Depth 1 | 20Hz | 障碍物信息 |
| `/perception/terrain_info` | `perception_msgs/msg/TerrainInfo` | Perception | Best Effort, Depth 1 | 10Hz | 地形信息 |
| `/vslam/pose` | `geometry_msgs/msg/PoseWithCovarianceStamped` | VSLAM | Best Effort, Depth 1 | 30Hz | 视觉定位位姿 |
| `/lidar_slam/pose` | `geometry_msgs/msg/PoseWithCovarianceStamped` | Lidar-SLAM | Best Effort, Depth 1 | 20Hz | 激光定位位姿 |
| `/map_manager/global_map` | `nav_msgs/msg/OccupancyGrid` | MapManager | Reliable + Transient Local | 事件驱动 | 全局地图 |
| `/map_manager/elevation_map` | `grid_map_msgs/msg/GridMap` | MapManager | Best Effort, Depth 1 | 1Hz | 高程地图 |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/pnc/set_goal` | `pnc_msgs/srv/SetGoal` | TE / Agent | 设置导航目标 |
| `/pnc/get_pnc_state` | `pnc_msgs/srv/GetPncState` | 任意模块 | 查询 PnC 状态 |
| `/pnc/cancel_navigation` | `pnc_msgs/srv/CancelNavigation` | TE / Gateway | 取消导航 |
| `/pnc/get_health_status` | `pnc_msgs/srv/GetHealthStatus` | EM / HDS | 健康查询 |

#### Actions

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/pnc/navigate_to` | `pnc_msgs/action/NavigateTo` | TE | 导航到目标点 |

---

## 4. 内部设计

### 4.1 节点架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      PlanningControlNode                                │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Planning Thread (50-100Hz)                   │   │
│  │                                                                 │   │
│  │  ┌──────────────┐    ┌──────────────┐    ┌──────────────────┐  │   │
│  │  │ Global       │    │ Local        │    │ Terrain          │  │   │
│  │  │ Planner      │───►│ Planner      │◄──►│ Analyzer         │  │   │
│  │  │              │    │              │    │                  │  │   │
│  │  │ - A* / Dijkstra│   │ - DWA / MPC  │    │ - 坡度计算       │  │   │
│  │  │ - RRT* (可选) │   │ - TEB (可选) │    │ - 粗糙度评估     │  │   │
│  │  │ - 地图搜索   │    │ - 轨迹优化   │    │ - 可通行性     │  │   │
│  │  └──────────────┘    └──────────────┘    └──────────────────┘  │   │
│  │           │                   │                                 │   │
│  │           ▼                   ▼                                 │   │
│  │  ┌──────────────────────────────────────────────────────────┐  │   │
│  │  │              Path Merger & Smoother                      │  │   │
│  │  │  - 融合全局路径 + 局部路径                               │  │   │
│  │  │  - 速度曲线平滑                                          │  │   │
│  │  └──────────────────────────────────────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                              │                                          │
│                              ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                 Control Thread (100Hz)                          │   │
│  │                                                                 │   │
│  │  ┌──────────────┐    ┌──────────────┐    ┌──────────────────┐  │   │
│  │  │ State        │    │ Velocity     │    │ Foothold         │  │   │
│  │  │ Estimator    │───►│ Generator    │◄──►│ Planner          │  │   │
│  │  │              │    │ (可选)       │    │ (可选)           │  │   │
│  │  │ - 位姿融合   │    │ - 纯跟踪     │    │ - 落脚点选择     │  │   │
│  │  │ - 速度估计   │    │ - 地形适应   │    │ - 地形评估       │  │   │
│  │  └──────────────┘    └──────────────┘    └──────────────────┘  │   │
│  │           │                   │                                 │   │
│  │           ▼                   ▼                                 │   │
│  │  ┌──────────────────────────────────────────────────────────┐  │   │
│  │  │              Command Publisher                             │  │   │
│  │  │  - 发布 /pnc/velocity_command (Twist)                    │  │   │
│  │  │  - 100Hz，Best Effort                                    │  │   │
│  │  └──────────────────────────────────────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                  ROS2 Callback Thread                           │   │
│  │                                                                 │   │
│  │  Subscribers: /sm/robot_state, /perception/..., /vslam/...     │   │
│  │  Publishers: /pnc/pnc_state, /pnc/path_info, ...               │   │
│  │  Services: set_goal, get_pnc_state, cancel_navigation          │   │
│  │  Action Server: navigate_to                                    │   │
│  │                                                                 │   │
│  │  ┌─────────────────────────────────────────────────────────┐   │   │
│  │  │              Navigation State Machine                   │   │   │
│  │  │  - 管理 PNC_IDLE → PNC_PLANNING → PNC_NAVIGATING ...    │   │   │
│  │  └─────────────────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.2 关键设计决策

1. **双线程架构**：规划线程（50-100Hz）与控制线程（100Hz）分离，避免规划耗时阻塞控制输出
2. **定位源优先级**：VSLAM 和 Lidar-SLAM 同时运行时，以 Lidar-SLAM 为主（室内精度更高），VSLAM 为辅（室外补充），通过协方差融合
3. **局部规划器选型**：默认 DWA（Dynamic Window Approach），复杂地形可切换 MPC（Model Predictive Control）
4. **地形适应策略**：平坦地形直接用速度指令；复杂地形启用 foothold planner，输出落脚点序列给 MC
5. **SM 状态本地缓存**：订阅 `/sm/robot_state`（Transient Local），缓存到原子变量，控制线程不调用 ROS2 Service

### 4.3 关键流程

#### 4.3.1 导航任务启动流程

```
TE 调用 /pnc/navigate_to Action（target_pose）
  → PnC 校验：
      1. SM 状态是否为 ACTIVE_WALKING / ACTIVE_MOTION？
      2. 目标点是否在地图范围内？
      3. 定位是否正常？
  → 校验通过：
      → 状态切换为 PNC_PLANNING
      → 调用 Global Planner 规划全局路径
      → 全局路径成功 → 状态切换为 PNC_NAVIGATING
      → 启动控制线程，开始发布 velocity_command
  → 校验失败：
      → 返回 Action Result（success=false, error_code）
```

#### 4.3.2 正常导航控制循环（100Hz）

```
控制线程每 10ms 执行：
  1. 读取当前定位位姿（VSLAM/Lidar-SLAM 融合）
  2. 读取最新全局路径和局部路径
  3. 读取障碍物信息（Perception）
  4. 轨迹跟踪：计算路径跟踪误差 → 生成基础速度指令
  5. 动态避障：检测路径上障碍物 → 调整速度/方向
  6. 地形适应（如启用）：
      - 读取地形信息
      - 调整足端高度、步态参数
      - 必要时调用 Foothold Planner
  7. 速度限制：
      - 线速度 < max_linear_vel
      - 角速度 < max_angular_vel
      - 加速度平滑（防止突变）
  8. 发布 /pnc/velocity_command (Twist)
  9. 检查是否到达目标：
      - 是 → 状态切换为 PNC_REACHED → Action 完成
      - 否 → 继续导航
```

#### 4.3.3 动态避障流程

```
Perception 检测到前方障碍物
  → 局部规划器评估：当前路径是否碰撞？
  → 碰撞：
      → 状态切换为 PNC_AVOIDING
      → 局部重规划（绕开障碍物）
      → 降低速度（安全优先）
      → 重规划成功 → 状态恢复 PNC_NAVIGATING
      → 重规划失败（连续 3 次）→ 状态切换为 PNC_FAILED
  → 无碰撞：
      → 继续正常导航
```

#### 4.3.4 导航取消流程

```
TE / Gateway 调用 /pnc/cancel_navigation Service
  → PnC 停止控制线程
  → 发布 velocity_command = 零速度（vx=0, vy=0, yaw_rate=0）
  → 状态切换为 PNC_IDLE
  → 正在执行的 NavigateTo Action 取消
  → MC 收到零速度 → 停止行走 → 保持站立
```

---

## 5. 与其他模块的交互

### 5.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|------|------|------|------|
| MC | PnC → MC | `/pnc/velocity_command` (Topic) | 行走控制信号 |
| SM | SM → PnC | `/sm/robot_state` (Topic) | 全局状态校验 |
| Perception | Perception → PnC | `/perception/obstacles` (Topic) | 障碍物信息 |
| Perception | Perception → PnC | `/perception/terrain_info` (Topic) | 地形信息 |
| VSLAM | VSLAM → PnC | `/vslam/pose` (Topic) | 视觉定位位姿 |
| Lidar-SLAM | Lidar-SLAM → PnC | `/lidar_slam/pose` (Topic) | 激光定位位姿 |
| MapManager | MapManager → PnC | `/map_manager/global_map` (Topic) | 全局地图 |
| MapManager | MapManager → PnC | `/map_manager/elevation_map` (Topic) | 高程地图 |
| TE | TE → PnC | `/pnc/navigate_to` (Action) | 导航任务 |
| TE | TE → PnC | `/pnc/cancel_navigation` (Service) | 取消导航 |
| HDS | PnC → HDS | `/pnc/heartbeat` (Topic) | 心跳 |
| DR | PnC → DR | `/pnc/path_info` (Topic) | 路径记录 |
| Gateway | Gateway → PnC | `/pnc/set_goal` (Service) | 设置目标点 |

### 5.2 关键交互时序

#### 导航任务完整时序

```
  Gateway/Agent   TE          SM           PnC         Perception    VSLAM      MC
      │            │           │            │            │            │         │
      │  "去门口"  │           │            │            │            │         │
      ├───────────►│           │            │            │            │         │
      │            │ RequestTransition      │            │            │         │
      │            │ (ACTIVE_WALKING)       │            │            │         │
      │            ├──────────►│            │            │            │         │
      │            │◄─accepted─┤            │            │            │         │
      │            │           │            │            │            │         │
      │            │ NavigateTo Action      │            │            │         │
      │            ├──────────►│            │            │            │         │
      │            │           │            │            │            │         │
      │            │           │            │ subscribe  │            │         │
      │            │           │            │◄───────────┤            │         │
      │            │           │            │◄───────────────────────┤         │
      │            │           │            │            │            │         │
      │            │           │            │ Planning...│            │         │
      │            │           │            │            │            │         │
      │            │           │            │ velocity_command       │         │
      │            │           │            ├──────────────────────────────────►│
      │            │           │            │  (100Hz)   │            │         │
      │            │           │            │            │            │         │
      │            │◄─Feedback─┤            │            │            │         │
      │            │ progress  │            │            │            │         │
      │            │           │            │            │            │         │
      │            │◄─Result───┤            │            │            │         │
      │            │ (reached) │            │            │            │         │
      │            │           │            │            │            │         │
```

---

## 6. 关键参数与配置

```yaml
# pnc/config/pnc_params.yaml

planning_control:
  ros__parameters:
    # 全局规划器
    global_planner:
      type: "a_star"              # a_star / dijkstra / rrt_star
      resolution: 0.05            # 地图分辨率 [m]
      inflation_radius: 0.3       # 障碍物膨胀半径 [m]
      timeout_sec: 5.0            # 规划超时 [s]
      max_replan_attempts: 3      # 最大重规划次数

    # 局部规划器
    local_planner:
      type: "dwa"                 # dwa / mpc / teb
      control_frequency: 100.0    # 控制频率 [Hz]
      max_linear_vel: 1.0         # 最大线速度 [m/s]
      max_angular_vel: 1.0        # 最大角速度 [rad/s]
      max_linear_accel: 2.0       # 最大线加速度 [m/s²]
      max_angular_accel: 3.0      # 最大角加速度 [rad/s²]
      sim_time: 2.0               # 前向模拟时间 [s]

    # MPC 专用参数（type=mpc 时生效）
    mpc:
      horizon_steps: 20           # 预测步数
      dt: 0.05                    # 预测步长 [s]
      weights:
        position: 10.0
        velocity: 1.0
        orientation: 5.0
        control: 0.1

    # 地形适应
    terrain_adaptation:
      enabled: true
      max_slope_deg: 15.0         # 最大可通行坡度 [deg]
      max_roughness: 0.05         # 最大粗糙度 [m]
      foothold_enabled: false     # 是否启用 foothold 规划（复杂地形）

    # 定位融合
    localization:
      primary_source: "lidar_slam"  # 主定位源
      secondary_source: "vslam"     # 辅助定位源
      fusion_method: "covariance"   # 融合方法

    # 到达判定
    goal_tolerance:
      position: 0.3               # 位置误差 [m]
      orientation: 0.2            # 姿态误差 [rad]
      velocity: 0.1               # 速度误差 [m/s]

    # 导航超时
    navigation_timeout_sec: 300.0   # 单次导航最大时间 [s]

    # 心跳频率
    heartbeat_rate_hz: 1.0

    # 安全校验
    enable_sm_check: true         # 导航前检查 SM 状态
    enable_obstacle_check: true   # 启用障碍物检测

    # 调试
    publish_path: true            # 是否发布路径信息
    publish_costmap: false        # 是否发布代价地图
```

---

## 7. 错误码定义

| 错误码 | 常量名 | 说明 | 严重级别 |
|--------|--------|------|---------|
| 0 | `OK` | 成功 | — |
| 4001 | `ERR_INVALID_GOAL` | 目标点无效（在障碍物内/不可达）| MEDIUM |
| 4002 | `ERR_GLOBAL_PLAN_FAILED` | 全局路径规划失败 | HIGH |
| 4003 | `ERR_LOCAL_PLAN_FAILED` | 局部路径规划失败 | HIGH |
| 4004 | `ERR_OBSTACLE_BLOCKED` | 被障碍物完全阻挡 | HIGH |
| 4005 | `ERR_LOCALIZATION_LOST` | 定位丢失（VSLAM + Lidar-SLAM 均失效）| CRITICAL |
| 4006 | `ERR_MAP_UNAVAILABLE` | 地图不可用 | HIGH |
| 4007 | `ERR_NAVIGATION_TIMEOUT` | 导航超时（超过 timeout_sec）| HIGH |
| 4008 | `ERR_MOTION_NOT_ALLOWED` | SM 状态不允许导航（非 ACTIVE_WALKING/MOTION）| HIGH |
| 4009 | `ERR_PATH_DEVIATION` | 路径偏离过大（定位漂移或打滑）| MEDIUM |
| 4010 | `ERR_REPLAN_EXHAUSTED` | 重规划次数耗尽 | HIGH |

---

## 8. 安全约束

### 8.1 SM 状态校验

- 导航任务启动前，检查 SM 状态是否为 `ACTIVE_WALKING` 或 `ACTIVE_MOTION`
- SM 状态变为 `ACTIVE_E_STOP` / `FAULT` / `SHUTTING_DOWN` 时：
  - 立即停止发布 velocity_command（或发布零速度）
  - 状态切换为 `PNC_IDLE`
  - 取消正在执行的 NavigateTo Action

### 8.2 速度限制

- 软件限速：`max_linear_vel` / `max_angular_vel` 硬约束
- 加速度平滑：控制输出经过低通滤波，防止速度突变导致机器人失衡
- 避障时自动降速：检测到障碍物时，线速度自动降低到安全范围

### 8.3 定位丢失保护

- 定位协方差超过阈值 → 触发 `ERR_LOCALIZATION_LOST`
- 定位丢失持续 > 3s → 停止导航，发布零速度，等待定位恢复

### 8.4 地形安全

- 坡度 > `max_slope_deg` 的区域标记为不可通行
- 粗糙度 > `max_roughness` 的区域降低速度或绕行
- Foothold 规划失败时，切换到保守步态（小步慢走）

---

## 9. 包结构

```
pnc_msgs/
├── msg/
│   ├── PncState.msg              # PnC 状态
│   ├── PathInfo.msg              # 路径信息
│   ├── TerrainCostmap.msg        # 地形代价地图
│   ├── PncErrorCode.msg          # 错误码
│   └── Heartbeat.msg             # 心跳
├── srv/
│   ├── SetGoal.srv               # 设置导航目标
│   ├── GetPncState.srv           # 查询 PnC 状态
│   ├── CancelNavigation.srv      # 取消导航
│   └── GetHealthStatus.srv       # 健康查询
├── action/
│   └── NavigateTo.action         # 导航到目标点
├── CMakeLists.txt
└── package.xml

pnc/
├── include/pnc/
│   ├── planning_control_node.hpp # 主节点类
│   ├── global_planner.hpp        # 全局规划器（A*/Dijkstra/RRT*）
│   ├── local_planner.hpp         # 局部规划器（DWA/MPC/TEB）
│   ├── velocity_generator.hpp    # 速度指令生成器
│   ├── state_estimator.hpp       # 定位状态估计（融合 VSLAM/Lidar-SLAM）
│   ├── terrain_analyzer.hpp      # 地形分析器
│   ├── foothold_planner.hpp      # 落脚点规划器（可选）
│   ├── path_merger.hpp           # 路径融合与平滑
│   └── obstacle_monitor.hpp      # 障碍物监控
├── src/
│   ├── planning_control_node.cpp
│   ├── global_planner.cpp
│   ├── local_planner.cpp
│   ├── velocity_generator.cpp
│   ├── state_estimator.cpp
│   ├── terrain_analyzer.cpp
│   ├── foothold_planner.cpp
│   ├── path_merger.cpp
│   └── obstacle_monitor.cpp
├── test/
│   ├── test_global_planner.cpp
│   ├── test_local_planner.cpp
│   ├── test_state_estimator.cpp
│   ├── test_terrain_analyzer.cpp
│   └── test_integration.cpp
├── config/
│   └── pnc_params.yaml           # 参数配置
├── launch/
│   └── pnc.launch.py
├── maps/                         # 预加载地图（可选）
│   └── default_map.yaml
├── CMakeLists.txt
└── package.xml
```

---

## 10. 关键性能指标（KPI）

| 指标 | 目标值 |
|------|--------|
| 控制输出频率 | 100Hz |
| 全局路径规划时间 | < 5s |
| 局部路径规划时间 | < 50ms |
| 定位融合延迟 | < 50ms |
| 障碍物检测响应 | < 100ms |
| 行走控制信号输出延迟 | < 10ms |
| 导航定位精度 | 位置误差 < 0.3m，姿态误差 < 0.2rad |
| 动态避障成功率 | > 95%（常见室内场景）|
| CPU 占用 | < 10%（单核）|
| 内存占用 | < 200MB |

---

## 附录：与 MC 的接口契约

PnC 和 MC 通过 `/pnc/velocity_command` Topic 交互：

| 字段 | 说明 | 范围 | 默认值 |
|------|------|------|--------|
| `linear.x` | 前后速度（前为正） | [-1.0, 1.0] m/s | 0.0 |
| `linear.y` | 左右速度（左为正） | [-0.5, 0.5] m/s | 0.0 |
| `angular.z` | 转角速度（逆时针为正） | [-1.0, 1.0] rad/s | 0.0 |

**关键约束**：
1. PnC 不直接输出关节指令，只输出高层速度指令
2. MC 的 RL 策略将速度指令转换为全身协调的关节力矩
3. PnC 发布频率 100Hz，MC 内部缓存并插值到 1kHz
4. PnC 停止发布（或发布零速度）时，MC 应在 100ms 内减速到停止
