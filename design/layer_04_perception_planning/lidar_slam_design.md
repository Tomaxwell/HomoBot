# Lidar-SLAM 模块设计

## 1. 模块概述与定位

**模块名称**：Lidar SLAM（激光雷达 SLAM）

**定位**：Lidar-SLAM 是端侧软件系统中的**激光雷达定位与建图引擎**，位于感知/规划层。它接收来自 HAL_Sensor 的 Lidar 原始点云数据，通过点云配准、位姿估计、地图构建，实时计算机器人在环境中的三维位姿，同时构建和维护全局一致的点云地图（或占据栅格地图）。Lidar-SLAM 为 PnC（导航）、MapManager（地图管理）提供鲁棒的定位信息，特别是在视觉受限场景（低光照、纹理缺失）下作为 VSLAM 的补充和备份。

**核心职责**：

1. **点云预处理**：接收原始点云，进行运动畸变校正、滤波、降采样
2. **扫描匹配（Scan Matching）**：通过 ICP/NDT 等算法进行帧间点云配准
3. **激光里程计（LO）**：基于扫描匹配计算帧间位姿变换
4. **后端优化**：关键帧位姿图优化，消除累积漂移
5. **回环检测**：基于点云描述子或几何位置的回环检测
6. **地图表示**：维护点云地图、占据栅格地图、高程地图
7. **定位模式**：在已有地图中仅定位不建图
8. **位姿发布**：实时发布机器人位姿和地图数据

**与相邻模块的边界**：

| 边界 | Lidar-SLAM 负责 | 对方负责 |
|------|----------------|---------|
| Lidar-SLAM ↔ HAL_Sensor | 订阅 Lidar 原始点云 | 硬件抽象、点云采集 |
| Lidar-SLAM ↔ TF | 查询 Lidar→base_link 变换、IMU 数据 | 坐标系管理 |
| Lidar-SLAM ↔ Perception | 接收滤波后点云/障碍物信息辅助 | 多传感器感知融合 |
| Lidar-SLAM ↔ PnC | 提供位姿、占据栅格地图、高程地图 | 路径规划与控制 |
| Lidar-SLAM ↔ MapManager | 提交点云地图/栅格地图；查询已有地图 | 地图存储、加载、生命周期 |
| Lidar-SLAM ↔ VSLAM | 位姿融合（松耦合）提升定位鲁棒性 | 视觉定位 |
| Lidar-SLAM ↔ HDS | 上报定位质量、Lidar 异常 | 故障诊断与定级 |

---

## 2. 职责边界

**Lidar-SLAM 不做的事情**（红线）：

- **不做视觉 SLAM** — 视觉特征提取、视觉里程计由 VSLAM 负责
- **不做路径规划** — 不输出导航路径或控制指令，只提供地图和位姿
- **不做运动控制** — 不直接控制机器人运动
- **不做目标识别** — 障碍物检测分类由 Perception 负责
- **不做云端通信** — 地图上传等通过 Gateway
- **不做语义理解** — 语义标签由 Perception 提供，Lidar-SLAM 仅使用几何信息

---

## 3. 状态机设计

### 3.1 Lidar-SLAM 状态枚举

| 状态 | 值 | 说明 |
|------|-----|------|
| `LIDAR_SLAM_INIT` | 0 | 初始化中，加载参数和地图 |
| `LIDAR_SLAM_NOT_READY` | 1 | 未就绪，等待有效点云帧 |
| `LIDAR_SLAM_OK` | 2 | 正常扫描匹配与建图 |
| `LIDAR_SLAM_LOST` | 3 | 扫描匹配丢失（点云退化或环境变化） |
| `LIDAR_SLAM_RELOCATING` | 4 | 正在重定位（从已有地图恢复） |
| `LIDAR_SLAM_LOOP_CLOSING` | 5 | 正在执行回环检测与全局优化 |
| `LIDAR_SLAM_DEGRADED` | 6 | 降级运行（仅激光里程计，无后端优化） |

### 3.2 状态转换图

```
                            ┌─────────────────────────────────────────────────────────┐
                            │                                                         │
                      ┌─────┴────────────┐  init_complete    ┌────────────────────┐   │
                ┌──►  │  LIDAR_SLAM      │──────────────────►│ LIDAR_SLAM_NOT_READY│   │
                │     │     INIT         │                   │         1           │   │
                │     │       0          │                   └─────────┬───────────┘   │
                │     └──────────────────┘                               │               │
                │           ▲                                            │ valid_cloud   │
                │           │            reset                           │               │
                │           │     ┌──────────────────────────────────────┘               │
                │           │     ▼                                                      │
                │           │  ┌──────────────────────┐    match_success    ┌───────────┐│
                │           │  │    LIDAR_SLAM_LOST   │◄───────────────────│LIDAR_SLAM_OK││
                │           │  │          3           │                   │     2     ││
                │           │  └──────────┬───────────┘                   └─────┬─────┘│
                │           │             │ match_fail                        │      │
                │           │      ┌──────┘                                   │      │
                │           │      ▼                                          │      │
                │           │  ┌──────────────────────┐              loop_detected    │
                │           └──│ LIDAR_SLAM_RELOCATING │◄─────────────────────────────┘
                │              │          4           │
                │              └──────────┬───────────┘
                │                         │
                │                         │ relocate_success
                │                         │
                │                         ▼
                │              ┌──────────────────────┐
                └─────────────│ LIDAR_SLAM_LOOP_CLOSING
                               │          5           │
                               └──────────┬───────────┘
                                          │ optimization_done
                                          ▼
                               ┌──────────────────────┐
                               │  LIDAR_SLAM_DEGRADED │◄──── backend_disabled
                               │          6           │
                               └──────────────────────┘
```

### 3.3 状态转换表

| 当前状态 | 目标状态 | 触发条件 | 优先级 | 说明 |
|----------|----------|----------|--------|------|
| INIT | NOT_READY | 参数加载完成，等待点云 | 60 | 正常启动 |
| INIT | DEGRADED | 参数加载失败 | 50 | 降级启动 |
| NOT_READY | OK | 收到有效点云并完成首帧配准 | 60 | 成功初始化 |
| NOT_READY | LOST | 首帧配准失败 | 60 | 无法初始化 |
| OK | LOST | 扫描匹配失败（退化场景） | 70 | 跟踪丢失 |
| OK | LOOP_CLOSING | 检测到回环 | 60 | 触发全局优化 |
| OK | DEGRADED | 后端优化线程崩溃 | 50 | 降级为纯 LO |
| LOST | RELOCATING | 启动重定位 | 70 | 尝试恢复 |
| RELOCATING | OK | 重定位成功 | 70 | 恢复跟踪 |
| RELOCATING | LOST | 重定位失败 | 70 | 持续丢失 |
| LOOP_CLOSING | OK | 全局优化完成 | 60 | 回到正常 |
| DEGRADED | OK | 后端恢复 | 50 | 恢复完整 SLAM |
| * | INIT | 用户重置/地图切换 | 100 | 最高优先级重置 |

### 3.4 状态转换约束

1. **退化检测**：检测到走廊、隧道等几何退化场景时，主动标记为 DEGRADED，提示 PnC 减速
2. **LOST 超时**：持续 LOST 超过 15s → 上报 HDS 定位丢失告警
3. **回环频率限制**：两次回环检测间隔至少 30s，避免过度优化
4. **状态变更时**发布 `/lidar_slam/lidar_slam_state` Topic

---

## 4. ROS2 接口定义

### 4.1 消息定义 (msg)

```
# lidar_slam_msgs/msg/LidarSlamState.msg
# Lidar-SLAM 运行状态

uint8 state                 # 当前状态
uint8 prev_state
builtin_interfaces/Time state_changed_at
uint32 keyframe_count       # 关键帧总数
uint32 map_point_count      # 地图点数
float32 matching_score      # 当前扫描匹配得分
bool is_degenerate          # 是否处于退化场景
```

```
# lidar_slam_msgs/msg/PoseWithCovariance.msg
# 带协方差的位姿

builtin_interfaces/Time stamp
string frame_id
geometry_msgs/Pose pose
float64[36] covariance
float32 confidence
```

```
# lidar_slam_msgs/msg/OccupancyGrid3D.msg
# 3D 占据栅格地图

builtin_interfaces/Time stamp
string frame_id
float32 resolution          # 栅格分辨率（m）
geometry_msgs/Point origin  # 地图原点
int32 size_x                # X方向栅格数
int32 size_y                # Y方向栅格数
int32 size_z                # Z方向栅格数
int8[] data                 # 占据值：-1=未知, 0=空闲, 100=占据
```

```
# lidar_slam_msgs/msg/ElevationMap.msg
# 高程地图（地形高度）

builtin_interfaces/Time stamp
string frame_id
float32 resolution
geometry_msgs/Point origin
int32 size_x
int32 size_y
float32[] elevation         # 每个栅格的高度（m）
float32[] variance          # 高度方差
bool[] is_valid             # 是否有效
```

```
# lidar_slam_msgs/msg/Keyframe.msg
# 关键帧信息

uint32 id
builtin_interfaces/Time stamp
geometry_msgs/Pose pose
sensor_msgs/PointCloud2 cloud    # 关键帧点云
float32 matching_score
```

```
# lidar_slam_msgs/msg/Heartbeat.msg
# Lidar-SLAM 心跳

builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message
float32 fps
```

### 4.2 服务定义 (srv)

```
# lidar_slam_msgs/srv/GetHealthStatus.srv

# Request（空）
---
# Response
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
uint32 keyframe_count
```

```
# lidar_slam_msgs/srv/SaveMap.srv
# 保存地图

string filepath
uint8 map_type
uint8 MAP_TYPE_POINTCLOUD = 0
uint8 MAP_TYPE_OCTOMAP    = 1
uint8 MAP_TYPE_GRID       = 2
---
# Response
bool success
uint16 error_code
string message
```

```
# lidar_slam_msgs/srv/LoadMap.srv
# 加载地图（进入定位模式）

string filepath
uint8 map_type
---
# Response
bool success
uint16 error_code
string message
```

```
# lidar_slam_msgs/srv/Reset.srv
# 重置

bool clear_map
---
# Response
bool success
uint16 error_code
string message
```

```
# lidar_slam_msgs/srv/GetPose.srv
# 查询当前位姿

# Request（空）
---
# Response
bool success
string message
PoseWithCovariance pose
```

```
# lidar_slam_msgs/srv/GetMap.srv
# 获取当前地图（用于 MapManager 同步）

uint8 map_type
---
# Response
bool success
uint16 error_code
string message
sensor_msgs/PointCloud2 pointcloud_map    # 点云地图
OccupancyGrid3D occupancy_map             # 占据栅格地图
ElevationMap elevation_map                # 高程地图
```

### 4.3 接口汇总表

#### Topics

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/lidar_slam/pose` | `lidar_slam_msgs/msg/PoseWithCovariance` | Lidar-SLAM → PnC | Reliable + Volatile + Depth 1 | 10 Hz | 实时位姿 |
| `/lidar_slam/occupancy_grid` | `lidar_slam_msgs/msg/OccupancyGrid3D` | Lidar-SLAM → PnC | Reliable + Volatile + Depth 1 | 1 Hz | 占据栅格地图 |
| `/lidar_slam/elevation_map` | `lidar_slam_msgs/msg/ElevationMap` | Lidar-SLAM → PnC | Reliable + Volatile + Depth 1 | 1 Hz | 高程地图 |
| `/lidar_slam/lidar_slam_state` | `lidar_slam_msgs/msg/LidarSlamState` | Lidar-SLAM → ALL | Reliable + Transient Local + Depth 1 | 事件驱动 | 状态广播 |
| `/lidar_slam/heartbeat` | `lidar_slam_msgs/msg/Heartbeat` | Lidar-SLAM → EM/HDS | Reliable + Volatile + Depth 1 | 1 Hz | 心跳 |

#### Subscribed Topics

| 名称 | 类型 | 来源 | 说明 |
|------|------|------|------|
| `/lidar/point_cloud` | `sensor_msgs/msg/PointCloud2` | HAL_Sensor | 原始点云 |
| `/imu/data` | `sensor_msgs/msg/Imu` | HAL_Sensor | IMU 数据（运动畸变校正） |
| `/tf` | `tf2_msgs/msg/TFMessage` | TF | 坐标变换 |
| `/perception/result` | `perception_msgs/msg/PerceptionResult` | Perception | 障碍物辅助 |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/lidar_slam/get_health_status` | `lidar_slam_msgs/srv/GetHealthStatus` | EM, HDS | 健康查询 |
| `/lidar_slam/save_map` | `lidar_slam_msgs/srv/SaveMap` | MapManager, TE | 保存地图 |
| `/lidar_slam/load_map` | `lidar_slam_msgs/srv/LoadMap` | MapManager, TE | 加载地图 |
| `/lidar_slam/reset` | `lidar_slam_msgs/srv/Reset` | TE, SM | 重置 |
| `/lidar_slam/get_pose` | `lidar_slam_msgs/srv/GetPose` | PnC | 查询位姿 |
| `/lidar_slam/get_map` | `lidar_slam_msgs/srv/GetMap` | MapManager | 获取地图 |

---

## 5. 内部设计

### 5.1 节点架构

```
┌──────────────────────────────────────────────────────────────────────────┐
│                         LidarSlamNode                                     │
│                                                                           │
│  ┌─────────────────────────────────────────────────────────────────────┐  │
│  │                      Point Cloud Preprocessor                        │  │
│  │  ┌───────────────┐  ┌───────────────┐  ┌───────────────────────┐   │  │
│  │  │  Motion       │  │  Filtering    │  │   Downsampling        │   │  │
│  │  │  Distortion   │  │  (Outlier/    │  │   (Voxel Grid)        │   │  │
│  │  │  Correction   │  │  Ground)      │  │                       │   │  │
│  │  │               │  │               │  │                       │   │  │
│  │  │ - IMU插值    │  │ - 离群点滤波  │  │ - 体素降采样          │   │  │
│  │  │ - 时间戳对齐  │  │ - 地面分割    │  │ - 均匀采样            │   │  │
│  │  └───────┬───────┘  └───────┬───────┘  └───────────┬───────────┘   │  │
│  │          │                  │                      │               │  │
│  │          └──────────────────┼──────────────────────┘               │  │
│  │                             │                                      │  │
│  │  ┌──────────────────────────▼──────────────────────────────────┐  │  │
│  │  │              Scan Matching (Frame-to-Frame)                    │  │  │
│  │  │  - ICP (Iterative Closest Point)                               │  │  │
│  │  │  - NDT (Normal Distributions Transform)                        │  │  │
│  │  │  - 或 ICP-NDT 混合策略                                         │  │  │
│  │  │  - 输出：帧间变换 T_{k}^{k-1}                                  │  │  │
│  │  └──────────────────────────┬──────────────────────────────────┘  │  │
│  └─────────────────────────────┼─────────────────────────────────────┘  │
│                                │                                         │
│  ┌─────────────────────────────▼─────────────────────────────────────┐  │
│  │                        Back-End                                      │  │
│  │  ┌───────────────┐  ┌───────────────┐  ┌───────────────────────┐  │  │
│  │  │  Keyframe     │  │  Local        │  │   Loop Detection      │  │  │
│  │  │  Manager      │  │  Optimization │  │   (Scan Context /     │  │  │
│  │  │               │  │               │  │    Geometric)         │  │  │
│  │  │ - 关键帧选取   │  │ - g2o/GTSAM   │  │ - Scan Context描述子  │  │  │
│  │  │ - 子地图管理   │  │ - 位姿图优化  │  │ - 几何位置验证        │  │  │
│  │  │ - 地图更新    │  │ - 子地图优化  │  │ - SE3变换估计         │  │  │
│  │  └───────┬───────┘  └───────┬───────┘  └───────────┬───────────┘  │  │
│  │          │                  │                      │              │  │
│  │          └──────────────────┼──────────────────────┘              │  │
│  │                             │                                     │  │
│  │  ┌──────────────────────────▼──────────────────────────────────┐  │  │
│  │  │              Global Optimization (Pose Graph)                  │  │  │
│  │  └─────────────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                        Map Representation                          │  │
│  │  ┌───────────────┐  ┌───────────────┐  ┌───────────────────────┐  │  │
│  │  │  Point Cloud  │  │  Occupancy    │  │   Elevation           │  │  │
│  │  │  Map          │  │  Grid 3D      │  │   Map                 │  │  │
│  │  │  (点云地图)    │  │  (占据栅格)    │  │   (高程地图)          │  │  │
│  │  └───────────────┘  └───────────────┘  └───────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                        Output Interface                            │  │
│  │  - /lidar_slam/pose                                               │  │
│  │  - /lidar_slam/occupancy_grid                                     │  │
│  │  - /lidar_slam/elevation_map                                      │  │
│  │  - /lidar_slam/lidar_slam_state                                   │  │
│  └───────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────────┘
```

### 5.2 关键设计决策

1. **NDT 为主，ICP 为辅**：室内/结构化场景使用 NDT（对初值不敏感），室外/非结构化场景使用 ICP（精度高）
2. **IMU 运动畸变校正**：使用 IMU 预积分对每帧点云进行去畸变，提升快速运动时的配准精度
3. **子地图策略**：维护滑动窗口子地图（最近 N 帧关键帧），匹配时与子地图而非单帧匹配，提高鲁棒性
4. **Scan Context 回环**：使用 Scan Context 描述子进行回环检测，对旋转鲁棒，适合室内外混合场景
5. **多地图表示**：同时维护点云地图（用于定位）、占据栅格（用于导航）、高程地图（用于地形分析）

### 5.3 关键流程

#### 5.3.1 扫描匹配流程

```
新点云帧到达
  → Motion Distortion Correction:
      → 读取 IMU 数据插值
      → 对每个点根据时间戳计算畸变补偿
  → Filtering:
      → 离群点去除（统计滤波）
      → 可选：地面分割（如果 Perception 未提供）
  → Downsampling:
      → Voxel Grid 降采样（如 0.05m）
  → Scan Matching:
      → 若上一帧成功：
          → 使用 IMU/匀速模型预测初始位姿
          → NDT 配准（粗配准）
          → ICP 精配准（细调）
          → 计算匹配得分
          → 得分 > 阈值 → 接受位姿
          → 得分 < 阈值 → 标记 LOST
      → 若上一帧丢失：
          → 进入 RELOCATING
          → 与关键帧数据库全局搜索
          → 找到候选关键帧后 ICP 验证
  → 发布 /lidar_slam/pose
```

#### 5.3.2 退化检测与处理

```
扫描匹配过程中
  → 计算点云协方差矩阵的特征值
  → 若某一方向特征值远小于其他方向：
      → 标记为退化场景（如长走廊、隧道）
      → 进入 DEGRADED 状态
      → 增加位姿协方差（该方向不确定性增大）
      → 发布退化警告给 PnC（建议减速/停止）
      → 若 VSLAM 可用，请求 VSLAM 位姿辅助
  → 退出退化场景后自动恢复 OK
```

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|------|------|------|------|
| HAL_Sensor | HAL → Lidar-SLAM | `/lidar/point_cloud` (Topic) | 原始点云 |
| HAL_Sensor | HAL → Lidar-SLAM | `/imu/data` (Topic) | IMU 数据 |
| TF | TF → Lidar-SLAM | `/tf`, `/tf_static` (Topic) | 坐标变换 |
| Perception | Perception → Lidar-SLAM | `/perception/result` (Topic) | 障碍物/地面信息 |
| PnC | Lidar-SLAM → PnC | `/lidar_slam/pose` (Topic) | 位姿 |
| PnC | Lidar-SLAM → PnC | `/lidar_slam/occupancy_grid` (Topic) | 占据栅格 |
| PnC | Lidar-SLAM → PnC | `/lidar_slam/elevation_map` (Topic) | 高程地图 |
| MapManager | Lidar-SLAM → MapManager | `/lidar_slam/save_map` (Service) | 保存地图 |
| MapManager | MapManager → Lidar-SLAM | `/lidar_slam/load_map` (Service) | 加载地图 |
| MapManager | MapManager → Lidar-SLAM | `/lidar_slam/get_map` (Service) | 获取地图 |
| VSLAM | Lidar-SLAM ↔ VSLAM | 位姿融合（松耦合 Topic） | 提升定位鲁棒性 |
| HDS | Lidar-SLAM → HDS | `/hds/report_diagnosis` (Service) | 异常上报 |
| EM | EM → Lidar-SLAM | `/lidar_slam/get_health_status` (Service) | 健康检查 |
| TE | TE → Lidar-SLAM | `/lidar_slam/reset` (Service) | 任务切换重置 |

### 6.2 关键交互时序

#### 时序：点云处理与导航

```
HAL_Sensor      Lidar-SLAM       PnC         MapManager
  │               │              │             │
  │─point_cloud──►│              │             │
  │─imu_data─────►│              │             │
  │               │              │             │
  │               │─预处理───────│             │
  │               │─扫描匹配─────│             │
  │               │              │             │
  │               │─pose────────►│             │
  │               │─occupancy───►│             │
  │               │─elevation───►│             │
  │               │              │             │
  │               │              │─路径规划────►│
  │               │              │             │
  │               │─save_map───────────────────►│
  │               │              │             │
```

---

## 7. 关键参数与配置

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `lidar_topic` | string | "/lidar/point_cloud" | 点云 Topic |
| `imu_topic` | string | "/imu/data" | IMU Topic |
| `scan_matching_method` | string | "ndt" | 扫描匹配方法（ndt/icp/hybrid） |
| `voxel_size` | float | 0.05 | 体素降采样分辨率（m） |
| `icp_max_iterations` | int | 30 | ICP 最大迭代次数 |
| `icp_max_correspondence_distance` | float | 1.0 | ICP 最大对应点距离（m） |
| `ndt_resolution` | float | 1.0 | NDT 分辨率（m） |
| `keyframe_min_distance` | float | 0.3 | 关键帧最小位移（m） |
| `keyframe_min_rotation` | float | 0.1 | 关键帧最小旋转（rad） |
| `loop_closure_enabled` | bool | true | 是否启用回环检测 |
| `loop_closure_min_interval_sec` | float | 30.0 | 回环检测最小间隔（s） |
| `occupancy_grid_resolution` | float | 0.1 | 占据栅格分辨率（m） |
| `elevation_map_resolution` | float | 0.1 | 高程地图分辨率（m） |
| `degenerate_threshold` | float | 0.01 | 退化检测阈值 |
| `map_save_path` | string | "/opt/robot/maps/lidar/" | 地图保存路径 |

---

## 8. 错误码定义

```
# lidar_slam_msgs/msg/ErrorCode.msg
uint16 OK                        = 0
uint16 ERR_INIT_FAILED           = 11001   # 初始化失败
uint16 ERR_SCAN_MATCHING_FAILED  = 11002   # 扫描匹配失败
uint16 ERR_RELOCATE_FAILED       = 11003   # 重定位失败
uint16 ERR_MAP_LOAD_FAILED       = 11004   # 地图加载失败
uint16 ERR_MAP_SAVE_FAILED       = 11005   # 地图保存失败
uint16 ERR_DEGENERATE_SCENE      = 11006   # 退化场景
uint16 ERR_LIDAR_TIMEOUT         = 11007   # Lidar 数据超时
uint16 ERR_IMU_TIMEOUT           = 11008   # IMU 数据超时
uint16 ERR_LOOP_DETECTION_FAILED = 11009   # 回环检测失败
uint16 ERR_OPTIMIZATION_FAILED   = 11010   # 优化失败
uint16 ERR_INVALID_MAP_FILE      = 11011   # 非法地图文件
uint16 ERR_POINT_CLOUD_EMPTY     = 11012   # 点云为空
uint16 ERR_TF_LOOKUP_FAILED      = 11013   # 坐标变换查询失败
uint16 ERR_NOT_IN_LOCALIZATION_MODE = 11014 # 非定位模式
uint16 ERR_RESET_FAILED          = 11015   # 重置失败
```

| 错误码 | 常量名 | 说明 | 严重级别 |
|--------|--------|------|----------|
| 0 | OK | 成功 | — |
| 11001 | ERR_INIT_FAILED | 初始化失败 | HIGH |
| 11002 | ERR_SCAN_MATCHING_FAILED | 扫描匹配失败 | HIGH |
| 11003 | ERR_RELOCATE_FAILED | 重定位失败 | MEDIUM |
| 11004 | ERR_MAP_LOAD_FAILED | 地图加载失败 | MEDIUM |
| 11005 | ERR_MAP_SAVE_FAILED | 地图保存失败 | LOW |
| 11006 | ERR_DEGENERATE_SCENE | 退化场景 | MEDIUM |
| 11007 | ERR_LIDAR_TIMEOUT | Lidar 数据超时 | HIGH |
| 11008 | ERR_IMU_TIMEOUT | IMU 数据超时 | MEDIUM |
| 11009 | ERR_LOOP_DETECTION_FAILED | 回环检测失败 | LOW |
| 11010 | ERR_OPTIMIZATION_FAILED | 优化失败 | MEDIUM |
| 11011 | ERR_INVALID_MAP_FILE | 非法地图文件 | MEDIUM |
| 11012 | ERR_POINT_CLOUD_EMPTY | 点云为空 | MEDIUM |
| 11013 | ERR_TF_LOOKUP_FAILED | 坐标变换查询失败 | MEDIUM |
| 11014 | ERR_NOT_IN_LOCALIZATION_MODE | 非定位模式 | LOW |
| 11015 | ERR_RESET_FAILED | 重置失败 | MEDIUM |

---

## 9. 安全约束

1. **位姿校验**：发布的位姿变化率必须在物理可行范围内（速度 < 5m/s，角速度 < 3rad/s），异常值丢弃
2. **协方差发布**：必须始终发布位姿协方差，退化场景下协方差增大提示 PnC
3. **跟踪丢失告警**：LOST 状态超过 15s 必须上报 HDS
4. **退化场景减速**：检测到退化场景时主动通知 PnC，建议减速或停止
5. **内存保护**：点云地图大小上限 500MB，超限后删除远处旧子地图
6. **Lidar 故障降级**：Lidar 数据超时 1s 后进入 DEGRADED，超时 5s 进入 LOST

---

## 10. 包结构

```
lidar_slam_msgs/        # 消息定义包
├── msg/
│   ├── LidarSlamState.msg
│   ├── PoseWithCovariance.msg
│   ├── OccupancyGrid3D.msg
│   ├── ElevationMap.msg
│   ├── Keyframe.msg
│   └── Heartbeat.msg
├── srv/
│   ├── GetHealthStatus.srv
│   ├── SaveMap.srv
│   ├── LoadMap.srv
│   ├── Reset.srv
│   ├── GetPose.srv
│   └── GetMap.srv
├── CMakeLists.txt
└── package.xml

lidar_slam/             # 节点实现包
├── include/lidar_slam/
│   ├── lidar_slam_node.hpp
│   ├── point_cloud_preprocessor.hpp
│   ├── motion_distortion_corrector.hpp
│   ├── scan_matcher.hpp
│   ├── lidar_odometry.hpp
│   ├── keyframe_manager.hpp
│   ├── local_optimizer.hpp
│   ├── loop_detector.hpp
│   ├── global_optimizer.hpp
│   ├── point_cloud_map.hpp
│   ├── occupancy_grid_map.hpp
│   └── elevation_map.hpp
├── src/
│   ├── lidar_slam_node.cpp
│   ├── point_cloud_preprocessor.cpp
│   ├── motion_distortion_corrector.cpp
│   ├── scan_matcher.cpp
│   ├── lidar_odometry.cpp
│   ├── keyframe_manager.cpp
│   ├── local_optimizer.cpp
│   ├── loop_detector.cpp
│   ├── global_optimizer.cpp
│   ├── point_cloud_map.cpp
│   ├── occupancy_grid_map.cpp
│   ├── elevation_map.cpp
│   └── main.cpp
├── test/
│   ├── test_scan_matching.cpp
│   ├── test_loop_closure.cpp
│   ├── test_map_representation.cpp
│   └── test_integration.cpp
├── config/
│   └── lidar_slam_params.yaml
├── launch/
│   └── lidar_slam.launch.py
├── CMakeLists.txt
└── package.xml
```

---

## 11. 关键性能指标 (KPI)

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 扫描匹配帧率 | > 10 Hz | 点云配准速度 |
| 定位延迟 | < 100ms | 点云到达 → 位姿发布 |
| 定位精度 | < 0.5% 漂移 | 行驶 100m 相对漂移 < 0.5m |
| 重定位成功率 | > 90% | 跟踪丢失后恢复成功率 |
| 回环检测召回率 | > 85% | 真实回环被检测到的比例 |
| 回环检测精确率 | > 99% | 检测为回环中真实回环的比例 |
| 退化检测准确率 | > 95% | 正确识别退化场景 |
| 占据栅格更新帧率 | > 1 Hz | 栅格地图更新频率 |
| 内存占用 | < 2GB | 含地图和点云缓存 |
| CPU 占用 | < 2 核 | NDT/ICP 计算 |
