# VSLAM 模块设计

## 1. 模块概述与定位

**模块名称**：Vision SLAM（视觉 SLAM）

**定位**：VSLAM 是端侧软件系统中的**视觉定位与建图引擎**，位于感知/规划层。它接收来自 HAL_Sensor 的相机图像数据，通过视觉特征提取、跟踪、位姿估计，实时计算机器人在环境中的六自由度位姿，同时构建和维护稀疏/半稠密视觉地图。VSLAM 为 PnC（导航）、Agent（环境理解）、Perception（语义融合）提供精确的定位信息。

**核心职责**：

1. **视觉前端**：接收相机图像，提取特征点（ORB/SuperPoint），进行帧间跟踪
2. **视觉里程计（VO）**：通过特征匹配或光流估计帧间位姿变换
3. **后端优化**：关键帧 BA（Bundle Adjustment）优化，消除累积漂移
4. **回环检测**：检测是否回到已访问位置，触发全局优化消除漂移
5. **定位模式**：在已有地图中仅定位不建图（重定位）
6. **地图管理**：维护稀疏点云地图和关键帧数据库
7. **位姿发布**：实时发布相机/基座位姿（`geometry_msgs/PoseStamped`）

**与相邻模块的边界**：

| 边界 | VSLAM 负责 | 对方负责 |
|------|-----------|---------|
| VSLAM ↔ HAL_Sensor | 订阅相机图像流 | 硬件抽象、图像采集 |
| VSLAM ↔ TF | 查询相机→base_link变换 | 坐标系管理 |
| VSLAM ↔ Perception | 接收语义分割辅助回环检测 | 视觉感知、语义理解 |
| VSLAM ↔ PnC | 提供定位位姿用于导航 | 路径规划与控制 |
| VSLAM ↔ MapManager | 提交视觉地图；查询已有地图 | 地图存储、加载、生命周期 |
| VSLAM ↔ Agent | 提供场景重识别结果 | VLA 决策 |
| VSLAM ↔ HDS | 上报定位质量、跟踪丢失 | 故障诊断与定级 |

---

## 2. 职责边界

**VSLAM 不做的事情**（红线）：

- **不做 Lidar SLAM** — 点云-based 的建图定位由 Lidar-SLAM 负责
- **不做路径规划** — 不输出导航路径，只输出位姿
- **不做运动控制** — 不直接控制机器人运动
- **不做感知识别** — 目标检测、语义分割由 Perception 负责，VSLAM 只使用轻量语义辅助
- **不做云端通信** — 地图同步等通过 Gateway
- **不做稠密重建** — 仅维护稀疏/半稠密地图，稠密重建非本模块职责

---

## 3. 状态机设计

### 3.1 VSLAM 状态枚举

| 状态 | 值 | 说明 |
|------|-----|------|
| `VSLAM_INIT` | 0 | 初始化中，加载参数和词典 |
| `VSLAM_NOT_READY` | 1 | 未就绪，等待初始化帧序列 |
| `VSLAM_OK` | 2 | 正常跟踪中 |
| `VSLAM_LOST` | 3 | 跟踪丢失（特征不足或运动过快） |
| `VSLAM_RELOCATING` | 4 | 正在重定位（尝试从已有地图恢复） |
| `VSLAM_LOOP_CLOSING` | 5 | 正在执行回环检测与全局优化 |
| `VSLAM_DEGRADED` | 6 | 降级（仅 VO，无后端优化） |

### 3.2 状态转换图

```
                            ┌─────────────────────────────────────────────────────────┐
                            │                                                         │
                      ┌─────┴────────┐  init_complete    ┌────────────────────────┐   │
                ┌──►  │   VSLAM      │──────────────────►│      VSLAM_NOT_READY   │   │
                │     │    INIT      │                   │           1            │   │
                │     │      0       │                   └───────────┬────────────┘   │
                │     └──────────────┘                               │                │
                │           ▲                                        │ enough_parallax│
                │           │            reset                       │                │
                │           │     ┌──────────────────────────────────┘                │
                │           │     ▼                                                   │
                │           │  ┌──────────────────┐    track_success    ┌───────────┐ │
                │           │  │    VSLAM_LOST    │◄───────────────────│  VSLAM_OK │ │
                │           │  │        3         │                   │     2     │ │
                │           │  └────────┬─────────┘                   └─────┬─────┘ │
                │           │           │ track_fail                       │       │
                │           │    ┌──────┘                                  │       │
                │           │    ▼                                         │       │
                │           │  ┌──────────────────┐              loop_detected     │
                │           └──│ VSLAM_RELOCATING │◄───────────────────────────────┘
                │              │        4         │
                │              └────────┬─────────┘
                │                       │
                │                       │ relocate_success
                │                       │
                │                       ▼
                │              ┌──────────────────┐
                └─────────────│  VSLAM_LOOP_CLOSING
                               │        5         │
                               └────────┬─────────┘
                                        │ optimization_done
                                        ▼
                               ┌──────────────────┐
                               │  VSLAM_DEGRADED  │◄──── backend_disabled
                               │        6         │
                               └──────────────────┘
```

### 3.3 状态转换表

| 当前状态 | 目标状态 | 触发条件 | 优先级 | 说明 |
|----------|----------|----------|--------|------|
| INIT | NOT_READY | 参数加载完成，等待初始化 | 60 | 正常启动 |
| INIT | DEGRADED | 词典加载失败 | 50 | 降级启动 |
| NOT_READY | OK | 初始化成功（有足够视差） | 60 | 成功初始化 |
| NOT_READY | LOST | 初始化失败 | 60 | 无法初始化 |
| OK | LOST | 跟踪失败（特征不足/运动模糊） | 70 | 跟踪丢失 |
| OK | LOOP_CLOSING | 检测到回环 | 60 | 触发全局优化 |
| OK | DEGRADED | 后端优化线程崩溃 | 50 | 降级为纯 VO |
| LOST | RELOCATING | 启动重定位流程 | 70 | 尝试恢复 |
| RELOCATING | OK | 重定位成功 | 70 | 恢复跟踪 |
| RELOCATING | LOST | 重定位失败 | 70 | 持续丢失 |
| LOOP_CLOSING | OK | 全局 BA 完成 | 60 | 回到正常跟踪 |
| DEGRADED | OK | 后端恢复 | 50 | 恢复完整 SLAM |
| * | INIT | 用户重置/地图切换 | 100 | 最高优先级重置 |

### 3.4 状态转换约束

1. **LOST 超时**：持续 LOST 超过 10s → 上报 HDS 定位丢失告警
2. **RELOCATING 超时**：重定位超过 30s 未成功 → 进入 LOST，等待人工干预或重新初始化
3. **LOOP_CLOSING 互斥**：回环检测期间暂停新关键帧插入，避免优化期间地图不一致
4. **DEGRADED 降级策略**：纯 VO 模式下每 100 帧强制发布一次位姿不确定性估计

---

## 4. ROS2 接口定义

### 4.1 消息定义 (msg)

```
# vslam_msgs/msg/VslamState.msg
# VSLAM 运行状态

uint8 state                 # 当前状态
uint8 prev_state
builtin_interfaces/Time state_changed_at
uint32 tracked_features     # 当前跟踪的特征点数
uint32 keyframe_count       # 关键帧总数
uint32 map_point_count      # 地图点总数
float32 tracking_quality    # 跟踪质量评分 (0.0-1.0)
```

```
# vslam_msgs/msg/PoseWithCovariance.msg
# 带协方差的位姿（用于 PnC）

builtin_interfaces/Time stamp
string frame_id             # 参考坐标系（如 "map"）
geometry_msgs/Pose pose
float64[36] covariance      # 6x6 协方差矩阵 (x,y,z,roll,pitch,yaw)
float32 confidence          # 位姿置信度
```

```
# vslam_msgs/msg/Keyframe.msg
# 关键帧信息

uint32 id
builtin_interfaces/Time stamp
geometry_msgs/Pose pose
sensor_msgs/CompressedImage thumbnail   # 缩略图（可选）
uint32 feature_count
bool is_loop_keyframe       # 是否是回环关键帧
```

```
# vslam_msgs/msg/MapPoint.msg
# 地图点信息

uint32 id
geometry_msgs/Point position
uint8[3] color              # RGB 颜色
uint32 observation_count    # 被观测到的次数
float32 quality_score       # 质量评分
```

```
# vslam_msgs/msg/TrackingStatus.msg
# 跟踪状态详情

bool tracking_lost          # 是否丢失
uint32 consecutive_lost_frames  # 连续丢失帧数
float32 inlier_ratio        # 内点比率
float32 mean_reprojection_error  # 平均重投影误差
string last_failure_reason  # 上次失败原因
```

```
# vslam_msgs/msg/Heartbeat.msg
# VSLAM 心跳

builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message
float32 fps
```

### 4.2 服务定义 (srv)

```
# vslam_msgs/srv/GetHealthStatus.srv

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
uint32 map_point_count
```

```
# vslam_msgs/srv/SaveMap.srv
# 保存地图到文件

string filepath
---
# Response
bool success
uint16 error_code
string message
uint32 keyframes_saved
uint32 mappoints_saved
```

```
# vslam_msgs/srv/LoadMap.srv
# 从文件加载地图（进入纯定位模式）

string filepath
---
# Response
bool success
uint16 error_code
string message
uint32 keyframes_loaded
```

```
# vslam_msgs/srv/Reset.srv
# 重置 VSLAM（清空地图，重新初始化）

bool clear_map              # true = 清空地图重新建图；false = 仅重置位姿
---
# Response
bool success
uint16 error_code
string message
```

```
# vslam_msgs/srv/GetPose.srv
# 查询当前位姿

# Request（空）
---
# Response
bool success
string message
PoseWithCovariance pose
```

### 4.3 接口汇总表

#### Topics

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/vslam/pose` | `vslam_msgs/msg/PoseWithCovariance` | VSLAM → PnC/Agent | Reliable + Volatile + Depth 1 | 10-30 Hz | 实时位姿 |
| `/vslam/vslam_state` | `vslam_msgs/msg/VslamState` | VSLAM → ALL | Reliable + Transient Local + Depth 1 | 事件驱动 | 状态广播 |
| `/vslam/tracking_status` | `vslam_msgs/msg/TrackingStatus` | VSLAM → ALL | Reliable + Volatile + Depth 1 | 10 Hz | 跟踪状态 |
| `/vslam/heartbeat` | `vslam_msgs/msg/Heartbeat` | VSLAM → EM/HDS | Reliable + Volatile + Depth 1 | 1 Hz | 心跳 |

#### Subscribed Topics

| 名称 | 类型 | 来源 | 说明 |
|------|------|------|------|
| `/camera/image_raw` | `sensor_msgs/msg/Image` | HAL_Sensor | 原始图像 |
| `/camera/camera_info` | `sensor_msgs/msg/CameraInfo` | HAL_Sensor | 相机内参 |
| `/tf` | `tf2_msgs/msg/TFMessage` | TF | 坐标变换 |
| `/perception/semantics` | `perception_msgs/msg/SemanticSegment` | Perception | 语义分割（辅助回环） |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/vslam/get_health_status` | `vslam_msgs/srv/GetHealthStatus` | EM, HDS | 健康查询 |
| `/vslam/save_map` | `vslam_msgs/srv/SaveMap` | MapManager, TE | 保存地图 |
| `/vslam/load_map` | `vslam_msgs/srv/LoadMap` | MapManager, TE | 加载地图 |
| `/vslam/reset` | `vslam_msgs/srv/Reset` | TE, SM | 重置 VSLAM |
| `/vslam/get_pose` | `vslam_msgs/srv/GetPose` | PnC, Agent | 查询位姿 |

---

## 5. 内部设计

### 5.1 节点架构

```
┌──────────────────────────────────────────────────────────────────────────┐
│                           VslamNode                                       │
│                                                                           │
│  ┌─────────────────────────────────────────────────────────────────────┐  │
│  │                        Visual Front-End                              │  │
│  │  ┌───────────────┐  ┌───────────────┐  ┌───────────────────────┐   │  │
│  │  │  Feature      │  │  Feature      │  │   Motion Model        │   │  │
│  │  │  Extractor    │  │  Matcher      │  │   (IMU/匀速模型)       │   │  │
│  │  │               │  │               │  │                       │   │  │
│  │  │ - ORB/SuperPoint│ - KNN匹配     │  │ - 预测初始位姿         │   │  │
│  │  │ - 特征描述子   │  │ - 比值测试    │  │ - IMU预积分            │   │  │
│  │  └───────┬───────┘  └───────┬───────┘  └───────────┬───────────┘   │  │
│  │          │                  │                      │               │  │
│  │          └──────────────────┼──────────────────────┘               │  │
│  │                             │                                      │  │
│  │  ┌──────────────────────────▼──────────────────────────────────┐  │  │
│  │  │              Visual Odometry (Frame-to-Frame)                │  │  │
│  │  │  - PnP (Perspective-n-Point)                                 │  │  │
│  │  │  - Essential/Fundamental Matrix                              │  │  │
│  │  │  - 初始位姿估计 → 特征投影 → 优化                             │  │  │
│  │  └──────────────────────────┬──────────────────────────────────┘  │  │
│  └─────────────────────────────┼─────────────────────────────────────┘  │
│                                │                                         │
│  ┌─────────────────────────────▼─────────────────────────────────────┐  │
│  │                        Back-End                                      │  │
│  │  ┌───────────────┐  ┌───────────────┐  ┌───────────────────────┐  │  │
│  │  │  Keyframe     │  │  Local BA     │  │   Loop Detection      │  │  │
│  │  │  Manager      │  │  (局部优化)    │  │   (回环检测)           │  │  │
│  │  │               │  │               │  │                       │  │  │
│  │  │ - 关键帧插入   │  │ - g2o/GTSAM   │  │ - DBoW3 词袋模型       │  │  │
│  │  │ - 共视图维护   │  │ - 滑动窗口优化 │  │ - 几何验证             │  │  │
│  │  │ - 地图点三角化 │  │ - 边缘化      │  │ - Sim3 相似变换估计    │  │  │
│  │  └───────┬───────┘  └───────┬───────┘  └───────────┬───────────┘  │  │
│  │          │                  │                      │              │  │
│  │          └──────────────────┼──────────────────────┘              │  │
│  │                             │                                     │  │
│  │  ┌──────────────────────────▼──────────────────────────────────┐  │  │
│  │  │              Global BA / Pose Graph Optimization               │  │  │
│  │  │  - 回环闭合后全局优化                                          │  │  │
│  │  │  - 位姿图优化（g2o/GTSAM）                                     │  │  │
│  │  └─────────────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                        Map Manager (Internal)                      │  │
│  │  - 地图点增删（基于观测质量）                                       │  │
│  │  - 关键帧数据库（用于重定位）                                        │  │
│  │  - 地图序列化/反序列化                                              │  │
│  └───────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  ┌───────────────────────────────────────────────────────────────────┐  │
│  │                        Output Interface                            │  │
│  │  - /vslam/pose                                                    │  │
│  │  - /vslam/tracking_status                                         │  │
│  │  - /vslam/vslam_state                                             │  │
│  └───────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────────┘
```

### 5.2 关键设计决策

1. **多线程架构**：前端跟踪与后端优化分离到不同线程，前端 30Hz 不阻塞，后端按需优化
2. **ORB + SuperPoint 双模式**：室内/静态场景用 ORB（快），室外/动态场景用 SuperPoint（鲁棒）
3. **IMU 融合**：使用 IMU 预积分提供运动先验，加速特征匹配初始化，提升快速运动鲁棒性
4. **词袋回环**：使用 DBoW3 离线训练词袋模型，支持快速图像相似度查询
5. **关键帧策略**：基于共视特征数和场景变化率自适应选择关键帧，避免冗余

### 5.3 关键流程

#### 5.3.1 跟踪流程

```
新图像帧到达
  → Feature Extractor 提取特征点
  → 若上一帧跟踪成功：
      → Motion Model 预测初始位姿
      → Feature Matcher 与上一帧匹配
      → PnP 求解当前帧位姿
      → 重投影误差优化（g2o 局部 BA）
      → 更新地图点观测
      → 发布 /vslam/pose
  → 若上一帧跟踪丢失：
      → 进入 RELOCATING 状态
      → Keyframe Database 中搜索相似关键帧
      → 特征匹配 + EPnP 求解位姿
      → 重定位成功 → 恢复 OK 状态
      → 重定位失败 → 持续 RELOCATING
  → 跟踪质量检查：
      → 内点比率 < 30% → 标记跟踪不稳定
      → 连续 5 帧不稳定 → 进入 LOST
```

#### 5.3.2 回环检测流程

```
新关键帧插入
  → 送入 Loop Detection 线程（异步）
  → DBoW3 计算词袋向量
  → 查询数据库中相似关键帧（Top-K）
  → 几何验证（特征匹配 + Sim3 估计）
  → 验证通过：
      → 进入 LOOP_CLOSING 状态
      → 融合重复地图点
      → 执行 Pose Graph Optimization
      → 可选：全局 BA（低频次）
      → 发布更新后的位姿
      → 状态恢复 OK
```

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|------|------|------|------|
| HAL_Sensor | HAL → VSLAM | `/camera/image_raw` (Topic) | 原始图像 |
| HAL_Sensor | HAL → VSLAM | `/camera/camera_info` (Topic) | 相机内参 |
| TF | TF → VSLAM | `/tf`, `/tf_static` (Topic) | 相机→base_link 变换 |
| Perception | Perception → VSLAM | `/perception/semantics` (Topic) | 语义分割辅助 |
| PnC | VSLAM → PnC | `/vslam/pose` (Topic) | 定位位姿 |
| PnC | PnC → VSLAM | `/vslam/get_pose` (Service) | 查询位姿 |
| MapManager | VSLAM → MapManager | `/vslam/save_map` (Service) | 保存地图 |
| MapManager | MapManager → VSLAM | `/vslam/load_map` (Service) | 加载地图 |
| Agent | VSLAM → Agent | `/vslam/pose` (Topic) | 位姿用于场景理解 |
| HDS | VSLAM → HDS | `/hds/report_diagnosis` (Service) | 跟踪丢失上报 |
| EM | EM → VSLAM | `/vslam/get_health_status` (Service) | 健康检查 |
| TE | TE → VSLAM | `/vslam/reset` (Service) | 任务切换时重置 |

### 6.2 关键交互时序

#### 时序：定位数据流

```
HAL_Sensor      VSLAM         PnC        MapManager
  │              │             │            │
  │─image_raw───►│             │            │
  │              │─VO跟踪─────►│            │
  │              │             │            │
  │              │─pose───────►│            │
  │              │             │─路径规划───►│
  │              │             │            │
  │              │             │            │
  │              │─save_map───────────────►│
  │              │             │            │
  │              │             │            │
```

---

## 7. 关键参数与配置

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `camera_topic` | string | "/camera/image_raw" | 图像 Topic |
| `camera_info_topic` | string | "/camera/camera_info" | 相机参数 Topic |
| `feature_type` | string | "orb" | 特征类型（orb/superpoint） |
| `num_features` | int | 1000 | 每帧提取特征点数 |
| `scale_factor` | float | 1.2 | 图像金字塔缩放因子 |
| `num_levels` | int | 8 | 金字塔层数 |
| `tracking_fps_target` | float | 30.0 | 目标跟踪帧率 |
| `keyframe_min_parallax` | float | 0.05 | 关键帧最小视差（弧度） |
| `loop_closure_enabled` | bool | true | 是否启用回环检测 |
| `vocabulary_path` | string | "/opt/robot/vocab/ORBvoc.txt" | 词袋模型路径 |
| `local_ba_window_size` | int | 10 | 局部 BA 滑动窗口大小 |
| `reprojection_threshold` | float | 3.0 | 重投影误差阈值（像素） |
| `imu_fusion_enabled` | bool | true | 是否融合 IMU |
| `map_save_path` | string | "/opt/robot/maps/vslam/" | 地图保存路径 |

---

## 8. 错误码定义

```
# vslam_msgs/msg/ErrorCode.msg
uint16 OK                        = 0
uint16 ERR_INIT_FAILED           = 10001   # 初始化失败
uint16 ERR_TRACKING_LOST         = 10002   # 跟踪丢失
uint16 ERR_RELOCATE_FAILED       = 10003   # 重定位失败
uint16 ERR_MAP_LOAD_FAILED       = 10004   # 地图加载失败
uint16 ERR_MAP_SAVE_FAILED       = 10005   # 地图保存失败
uint16 ERR_VOCABULARY_LOAD_FAILED = 10006  # 词袋模型加载失败
uint16 ERR_NOT_ENOUGH_FEATURES   = 10007   # 特征点不足
uint16 ERR_CAMERA_CALIB_INVALID  = 10008   # 相机标定无效
uint16 ERR_LOOP_DETECTION_FAILED = 10009   # 回环检测失败
uint16 ERR_OPTIMIZATION_FAILED   = 10010   # 优化失败
uint16 ERR_INVALID_MAP_FILE      = 10011   # 非法地图文件
uint16 ERR_IMU_INTEGRATION_FAILED = 10012  # IMU 预积分失败
uint16 ERR_TF_LOOKUP_FAILED      = 10013   # 坐标变换查询失败
uint16 ERR_NOT_IN_LOCALIZATION_MODE = 10014 # 非定位模式
uint16 ERR_RESET_FAILED          = 10015   # 重置失败
```

| 错误码 | 常量名 | 说明 | 严重级别 |
|--------|--------|------|----------|
| 0 | OK | 成功 | — |
| 10001 | ERR_INIT_FAILED | 初始化失败 | HIGH |
| 10002 | ERR_TRACKING_LOST | 跟踪丢失 | MEDIUM |
| 10003 | ERR_RELOCATE_FAILED | 重定位失败 | MEDIUM |
| 10004 | ERR_MAP_LOAD_FAILED | 地图加载失败 | MEDIUM |
| 10005 | ERR_MAP_SAVE_FAILED | 地图保存失败 | LOW |
| 10006 | ERR_VOCABULARY_LOAD_FAILED | 词袋模型加载失败 | HIGH |
| 10007 | ERR_NOT_ENOUGH_FEATURES | 特征点不足 | MEDIUM |
| 10008 | ERR_CAMERA_CALIB_INVALID | 相机标定无效 | HIGH |
| 10009 | ERR_LOOP_DETECTION_FAILED | 回环检测失败 | LOW |
| 10010 | ERR_OPTIMIZATION_FAILED | 优化失败 | MEDIUM |
| 10011 | ERR_INVALID_MAP_FILE | 非法地图文件 | MEDIUM |
| 10012 | ERR_IMU_INTEGRATION_FAILED | IMU 预积分失败 | MEDIUM |
| 10013 | ERR_TF_LOOKUP_FAILED | 坐标变换查询失败 | MEDIUM |
| 10014 | ERR_NOT_IN_LOCALIZATION_MODE | 非定位模式 | LOW |
| 10015 | ERR_RESET_FAILED | 重置失败 | MEDIUM |

---

## 9. 安全约束

1. **位姿校验**：发布的位姿必须在合理范围（距离上一帧位移 < 5m，旋转 < 90°），异常值丢弃
2. **协方差发布**：必须始终发布位姿协方差，PnC 据此判断定位可信度
3. **跟踪丢失告警**：LOST 状态超过 10s 必须上报 HDS，同时发布 covariance → 无穷大
4. **多线程安全**：前端、后端、回环检测线程共享地图数据时必须加锁保护
5. **内存保护**：地图点数量上限 100 万，超限后删除低质量旧点
6. **相机故障降级**：Camera 数据超时 500ms 后自动进入 DEGRADED，停止发布位姿

---

## 10. 包结构

```
vslam_msgs/             # 消息定义包
├── msg/
│   ├── VslamState.msg
│   ├── PoseWithCovariance.msg
│   ├── Keyframe.msg
│   ├── MapPoint.msg
│   ├── TrackingStatus.msg
│   └── Heartbeat.msg
├── srv/
│   ├── GetHealthStatus.srv
│   ├── SaveMap.srv
│   ├── LoadMap.srv
│   ├── Reset.srv
│   └── GetPose.srv
├── CMakeLists.txt
└── package.xml

vslam/                  # 节点实现包
├── include/vslam/
│   ├── vslam_node.hpp
│   ├── visual_frontend.hpp
│   ├── feature_extractor.hpp
│   ├── feature_matcher.hpp
│   ├── visual_odometry.hpp
│   ├── keyframe_manager.hpp
│   ├── local_ba.hpp
│   ├── loop_detector.hpp
│   ├── global_ba.hpp
│   ├── map.hpp
│   └── imu_integrator.hpp
├── src/
│   ├── vslam_node.cpp
│   ├── visual_frontend.cpp
│   ├── feature_extractor.cpp
│   ├── feature_matcher.cpp
│   ├── visual_odometry.cpp
│   ├── keyframe_manager.cpp
│   ├── local_ba.cpp
│   ├── loop_detector.cpp
│   ├── global_ba.cpp
│   ├── map.cpp
│   ├── imu_integrator.cpp
│   └── main.cpp
├── test/
│   ├── test_feature_extraction.cpp
│   ├── test_vo.cpp
│   ├── test_loop_closure.cpp
│   └── test_integration.cpp
├── config/
│   └── vslam_params.yaml
├── launch/
│   └── vslam.launch.py
├── CMakeLists.txt
└── package.xml
```

---

## 11. 关键性能指标 (KPI)

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 跟踪帧率 | > 20 FPS | 视觉前端处理速度 |
| 跟踪延迟 | < 50ms | 图像到达 → 位姿发布 |
| 定位精度 | < 1% 漂移 | 行驶 100m 相对漂移 < 1m |
| 重定位成功率 | > 95% | 跟踪丢失后恢复成功率 |
| 回环检测召回率 | > 90% | 真实回环被检测到的比例 |
| 回环检测精确率 | > 99% | 检测为回环中真实回环的比例 |
| 地图点数量 | < 1,000,000 | 稳态地图规模上限 |
| 内存占用 | < 2GB | 含地图和词袋模型 |
| GPU 利用率 | < 60% | SuperPoint 推理时 |
| 位姿发布帧率 | 10-30 Hz | 与相机帧率匹配 |
