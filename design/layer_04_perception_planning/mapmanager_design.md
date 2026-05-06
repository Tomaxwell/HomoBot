# MapManager 模块设计

## 1. 模块概述与定位

**模块名称**：Map Manager（地图管理）

**定位**：MapManager 是端侧软件系统中的**地图生命周期管理中心**，位于感知/规划层。它不直接执行 SLAM 算法，而是负责接收、存储、加载、切换和生命周期管理来自 VSLAM、Lidar-SLAM 的地图数据，为 PnC（导航）、TE（任务执行）提供统一的地图查询和切换服务。MapManager 是地图数据的"档案管理员"和"调度中心"。

**核心职责**：

1. **地图存储管理**：持久化存储视觉地图、点云地图、占据栅格地图、高程地图
2. **地图加载与切换**：根据任务需求加载对应地图，支持运行时切换
3. **地图生命周期**：创建、保存、删除、归档、版本管理
4. **多地图融合**：融合 VSLAM 和 Lidar-SLAM 的地图数据，生成统一地图
5. **地图查询服务**：提供按区域、按类型、按时间查询地图的接口
6. **地图同步**：通过 Gateway 与云端地图服务同步（上传/下载）
7. **地图健康监控**：监控地图质量（一致性、完整性），标记过期地图

**与相邻模块的边界**：

| 边界 | MapManager 负责 | 对方负责 |
|------|----------------|---------|
| MapManager ↔ VSLAM | 加载/保存视觉地图；查询视觉定位结果 | 视觉 SLAM 算法、位姿估计 |
| MapManager ↔ Lidar-SLAM | 加载/保存点云/栅格地图；查询激光定位结果 | 激光 SLAM 算法、扫描匹配 |
| MapManager ↔ PnC | 提供当前激活地图用于导航规划 | 路径规划、避障、导航执行 |
| MapManager ↔ TE | 根据任务切换地图；查询地图元数据 | 任务调度与执行 |
| MapManager ↔ Gateway | 上传/下载地图；同步地图列表 | 云端通信 |
| MapManager ↔ Setting | 读取地图存储路径、大小限制等配置 | 参数持久化 |
| MapManager ↔ HDS | 上报地图异常（损坏、不一致） | 故障诊断与定级 |

---

## 2. 职责边界

**MapManager 不做的事情**（红线）：

- **不做 SLAM 算法** — 不执行特征提取、扫描匹配、位姿估计，那是 VSLAM/Lidar-SLAM 的职责
- **不做路径规划** — 不计算导航路径，只提供地图数据给 PnC
- **不做实时定位** — 不输出实时位姿，位姿由 VSLAM/Lidar-SLAM 输出
- **不做云端通信** — 地图云端同步通过 Gateway
- **不做数据压缩/加密** — 如需压缩加密由 Gateway 或专用工具处理
- **不做感知识别** — 不在地图中标注语义目标，语义由 Perception 提供

---

## 3. 状态机设计

### 3.1 地图管理状态枚举

MapManager 本身状态简单，主要管理"地图"对象的状态：

| 状态 | 值 | 说明 |
|------|-----|------|
| `MM_IDLE` | 0 | 空闲，无激活地图 |
| `MM_LOADING` | 1 | 正在加载地图 |
| `MM_ACTIVE` | 2 | 地图已激活，可被 PnC 使用 |
| `MM_SAVING` | 3 | 正在保存地图 |
| `MM_SYNCING` | 4 | 正在与云端同步地图 |
| `MM_ERROR` | 5 | 地图操作出错 |

### 3.2 地图对象生命周期状态

每个地图对象有自己的生命周期状态：

| 状态 | 说明 |
|------|------|
| `MAP_CREATING` | 正在由 SLAM 创建中 |
| `MAP_ACTIVE` | 当前正在使用的激活地图 |
| `MAP_SAVED` | 已保存到磁盘，非激活 |
| `MAP_ARCHIVED` | 已归档（旧版本） |
| `MAP_CORRUPTED` | 文件损坏或不一致 |
| `MAP_DELETED` | 已标记删除（待清理） |

---

## 4. ROS2 接口定义

### 4.1 消息定义 (msg)

```
# mapmanager_msgs/msg/MapManagerState.msg
# MapManager 状态

uint8 state
uint8 prev_state
builtin_interfaces/Time state_changed_at
string active_map_id        # 当前激活地图ID
uint32 total_maps           # 本地存储的地图总数
uint64 total_storage_bytes  # 总存储占用（字节）
```

```
# mapmanager_msgs/msg/MapInfo.msg
# 地图元数据

string map_id               # 地图唯一ID（UUID）
string name                 # 地图名称（用户可读）
string description
builtin_interfaces/Time created_at
builtin_interfaces/Time modified_at
string created_by           # 创建来源（"vslam", "lidar_slam", "manual"）
uint8 map_type
uint8 MAP_TYPE_VISUAL    = 0
uint8 MAP_TYPE_POINTCLOUD = 1
uint8 MAP_TYPE_OCCUPANCY  = 2
uint8 MAP_TYPE_ELEVATION  = 3
uint8 MAP_TYPE_FUSED     = 4
uint64 file_size_bytes
string file_path
geometry_msgs/Point origin          # 地图原点（经纬度或局部坐标）
geometry_msgs/Vector3 extent        # 地图范围（长x宽x高，m）
float32 resolution
bool is_active              # 是否当前激活
uint8 quality_score         # 质量评分（0-100）
string version              # 地图版本
```

```
# mapmanager_msgs/msg/MapList.msg
# 地图列表

MapInfo[] maps
uint32 total_count
```

```
# mapmanager_msgs/msg/Heartbeat.msg
# MapManager 心跳

builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message
uint32 total_maps
uint64 total_storage_bytes
```

### 4.2 服务定义 (srv)

```
# mapmanager_msgs/srv/GetHealthStatus.srv

# Request（空）
---
# Response
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
uint32 total_maps
uint64 total_storage_bytes
```

```
# mapmanager_msgs/srv/SubmitMap.srv
# 提交新地图（通知 SLAM 开始建图）

string name
string description
uint8 map_type
---
# Response
bool success
uint16 error_code
string message
string map_id
```

```
# mapmanager_msgs/srv/LoadMap.srv
# 加载地图并激活

string map_id
---
# Response
bool success
uint16 error_code
string message
MapInfo map_info
```

```
# mapmanager_msgs/srv/SaveMap.srv
# 保存当前 SLAM 地图

string map_id
string name
---
# Response
bool success
uint16 error_code
string message
MapInfo map_info
```

```
# mapmanager_msgs/srv/RemoveMap.srv
# 移除地图

string map_id
bool confirm                # 必须传 true
---
# Response
bool success
uint16 error_code
string message
```

```
# mapmanager_msgs/srv/GetMapList.srv
# 获取地图列表

uint8 map_type_filter       # 0=全部
---
# Response
bool success
string message
MapList map_list
```

```
# mapmanager_msgs/srv/GetMapInfo.srv
# 查询地图详情

string map_id
---
# Response
bool success
string message
MapInfo map_info
```

```
# mapmanager_msgs/srv/SyncMapWithCloud.srv
# 与云端同步地图

string map_id
uint8 direction
uint8 DIRECTION_UPLOAD   = 0
uint8 DIRECTION_DOWNLOAD = 1
---
# Response
bool success
uint16 error_code
string message
uint64 bytes_transferred
```

```
# mapmanager_msgs/srv/GetActiveMap.srv
# 获取当前激活地图

# Request（空）
---
# Response
bool success
string message
bool has_active_map
MapInfo map_info
```

### 4.3 接口汇总表

#### Topics

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/mapmanager/state` | `mapmanager_msgs/msg/MapManagerState` | MapManager → ALL | Reliable + Transient Local + Depth 1 | 事件驱动 | 状态广播 |
| `/mapmanager/map_list` | `mapmanager_msgs/msg/MapList` | MapManager → ALL | Reliable + Transient Local + Depth 1 | 变更时 | 地图列表 |
| `/mapmanager/heartbeat` | `mapmanager_msgs/msg/Heartbeat` | MapManager → EM/HDS | Reliable + Volatile + Depth 1 | 1 Hz | 心跳 |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/mapmanager/get_health_status` | `mapmanager_msgs/srv/GetHealthStatus` | EM, HDS | 健康查询 |
| `/mapmanager/submit_map` | `mapmanager_msgs/srv/SubmitMap` | TE, Gateway | 创建新地图 |
| `/mapmanager/load_map` | `mapmanager_msgs/srv/LoadMap` | TE, Gateway | 加载并激活地图 |
| `/mapmanager/save_map` | `mapmanager_msgs/srv/SaveMap` | TE, VSLAM, Lidar-SLAM | 保存地图 |
| `/mapmanager/remove_map` | `mapmanager_msgs/srv/RemoveMap` | Gateway（管理员） | 删除地图 |
| `/mapmanager/get_map_list` | `mapmanager_msgs/srv/GetMapList` | Gateway, TE | 列出地图 |
| `/mapmanager/get_map_info` | `mapmanager_msgs/srv/GetMapInfo` | TE, PnC | 查询地图详情 |
| `/mapmanager/sync_map_with_cloud` | `mapmanager_msgs/srv/SyncMapWithCloud` | Gateway | 云端同步 |
| `/mapmanager/get_active_map` | `mapmanager_msgs/srv/GetActiveMap` | PnC, TE | 获取激活地图 |

---

## 5. 内部设计

### 5.1 节点架构

```
┌──────────────────────────────────────────────────────────────────────────┐
│                         MapManagerNode                                    │
│                                                                           │
│  ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐   │
│  │  Map Registry    │    │  Storage Manager │    │  Lifecycle       │   │
│  │  (地图注册表)     │    │  (存储管理)       │    │  Controller      │   │
│  │                  │    │                  │    │  (生命周期控制)   │   │
│  │  - 地图元数据     │    │  - 文件读写      │    │  - 创建→保存     │   │
│  │  - 索引查询      │    │  - 压缩/解压     │    │  - 加载→激活     │   │
│  │  - 版本管理      │    │  - 配额管理      │    │  - 归档→删除     │   │
│  └────────┬─────────┘    └────────┬─────────┘    └────────┬─────────┘   │
│           │                       │                       │             │
│  ┌────────▼───────────────────────▼───────────────────────▼─────────┐   │
│  │                         Map Fusion Engine                          │   │
│  │   (视觉地图 + 点云地图 → 统一融合地图)                              │   │
│  └────────┬───────────────────────────────────────────────────────┬───┘   │
│           │                                                       │       │
│  ┌────────▼─────────┐   ┌──────────────────┐   ┌────────────────▼───┐   │
│  │  Cloud Sync      │   │  Quality         │   │  Active Map        │   │
│  │  Agent           │   │  Monitor         │   │  Cache             │   │
│  │  (云端同步)       │   │  (质量监控)       │   │  (激活地图缓存)     │   │
│  │                  │   │                  │   │                    │   │
│  │  - 上传地图      │   │  - 一致性检查    │   │  - 内存映射        │   │
│  │  - 下载地图      │   │  - 完整性校验    │   │  - 快速查询        │   │
│  │  - 差异同步      │   │  - 过期标记      │   │  - 热切换支持      │   │
│  └──────────────────┘   └──────────────────┘   └────────────────────┘   │
│                                                                           │
│  ┌───────────────────────────────────────────────────────────────────┐   │
│  │                        ROS2 Service/Topic Interface               │   │
│  └───────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────────┘
```

### 5.2 关键设计决策

1. **懒加载策略**：地图文件不常驻内存，仅在 `LoadMap` 后加载到 Active Map Cache
2. **写时复制（COW）**：保存地图时不阻塞 SLAM 的当前建图，采用快照方式复制
3. **自动归档**：旧版本地图自动归档，保留最近 3 个版本
4. **存储配额**：总存储超过配额时，按 LRU 策略删除最久未用的非激活地图
5. **地图指纹**：每个地图文件计算 SHA256 指纹，用于完整性校验和去重

### 5.3 关键流程

#### 5.3.1 地图加载与激活流程

```
TE 调用 /mapmanager/load_map(map_id)
  → MapRegistry 查询 map_id 是否存在
    → 不存在 → 返回 ERR_MAP_NOT_FOUND
  → LifecycleController 检查当前状态
    → 当前有激活地图 → 通知 VSLAM/Lidar-SLAM 切换
  → StorageManager 从磁盘读取地图文件
  → 加载到 Active Map Cache
  → 通知 VSLAM /lidar_slam/load_map
  → 通知 PnC 地图已切换
  → 发布 /mapmanager/state (ACTIVE)
  → 返回 success
```

#### 5.3.2 地图保存流程

```
TE 调用 /mapmanager/save_map
  → LifecycleController 发送保存请求给 VSLAM/Lidar-SLAM
    → VSLAM 调用 /vslam/save_map
    → Lidar-SLAM 调用 /lidar_slam/save_map
  → StorageManager 接收地图文件
  → 计算 SHA256 指纹
  → 压缩（gzip）后写入磁盘
  → MapRegistry 更新元数据
  → 发布 /mapmanager/map_list 更新
  → 返回保存结果
```

#### 5.3.3 云端同步流程

```
Gateway 调用 /mapmanager/sync_map_with_cloud
  → CloudSyncAgent 检查方向
    → UPLOAD：读取本地地图 → 通过 Gateway 上传
    → DOWNLOAD：通过 Gateway 下载 → 验证指纹 → 写入磁盘
  → StorageManager 更新存储
  → MapRegistry 更新元数据（云端同步时间）
  → 返回同步结果
```

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|------|------|------|------|
| VSLAM | MapManager → VSLAM | `/vslam/load_map` (Service) | 加载视觉地图 |
| VSLAM | MapManager → VSLAM | `/vslam/save_map` (Service) | 保存视觉地图 |
| Lidar-SLAM | MapManager → Lidar-SLAM | `/lidar_slam/load_map` (Service) | 加载激光地图 |
| Lidar-SLAM | MapManager → Lidar-SLAM | `/lidar_slam/save_map` (Service) | 保存激光地图 |
| PnC | MapManager → PnC | `/mapmanager/get_active_map` (Service) | 获取激活地图 |
| PnC | MapManager → PnC | `/mapmanager/state` (Topic) | 地图状态变更通知 |
| TE | TE → MapManager | `/mapmanager/load_map` (Service) | 任务切换时加载地图 |
| TE | TE → MapManager | `/mapmanager/submit_map` (Service) | 创建新地图 |
| Gateway | Gateway → MapManager | `/mapmanager/sync_map_with_cloud` (Service) | 云端同步 |
| Gateway | MapManager → Gateway | `/mapmanager/map_list` (Topic) | 地图列表同步 |
| Setting | MapManager → Setting | `/setting/get_parameter` (Service) | 读取存储配置 |
| HDS | MapManager → HDS | `/hds/report_diagnosis` (Service) | 上报地图异常 |
| EM | EM → MapManager | `/mapmanager/get_health_status` (Service) | 健康检查 |

### 6.2 关键交互时序

#### 时序：任务切换时地图加载

```
TE           MapManager        VSLAM        Lidar-SLAM      PnC
 │              │               │              │             │
 │─load_map────►│               │              │             │
 │              │               │              │             │
 │              │─load_map─────►│              │             │
 │              │─load_map────────────────────►│             │
 │              │               │              │             │
 │              │◄─loaded──────│              │             │
 │              │◄─loaded─────────────────────│             │
 │              │               │              │             │
 │              │─state(ACTIVE)───────────────►│             │
 │              │─state(ACTIVE)─────────────────────────────►│
 │              │               │              │             │
 │◄─success────│               │              │             │
 │              │               │              │             │
```

---

## 7. 关键参数与配置

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `maps_directory` | string | "/opt/robot/maps/" | 地图存储根目录 |
| `max_storage_bytes` | int64 | 10737418240 | 最大存储配额（10GB） |
| `max_maps_count` | int | 100 | 最大地图数量 |
| `archive_versions` | int | 3 | 保留的历史版本数 |
| `compression_enabled` | bool | true | 是否启用压缩 |
| `auto_archive_days` | int | 30 | 自动归档天数 |
| `cloud_sync_enabled` | bool | true | 是否启用云端同步 |
| `integrity_check_on_load` | bool | true | 加载时是否校验完整性 |
| `active_map_cache_size_mb` | int | 512 | 激活地图缓存大小（MB） |

---

## 8. 错误码定义

```
# mapmanager_msgs/msg/ErrorCode.msg
uint16 OK                        = 0
uint16 ERR_MAP_NOT_FOUND         = 12001   # 地图不存在
uint16 ERR_MAP_LOAD_FAILED       = 12002   # 地图加载失败
uint16 ERR_MAP_SAVE_FAILED       = 12003   # 地图保存失败
uint16 ERR_MAP_DELETE_FAILED     = 12004   # 地图删除失败
uint16 ERR_STORAGE_FULL          = 12005   # 存储空间已满
uint16 ERR_MAP_CORRUPTED         = 12006   # 地图文件损坏
uint16 ERR_INVALID_MAP_TYPE      = 12007   # 非法地图类型
uint16 ERR_SYNC_FAILED           = 12008   # 云端同步失败
uint16 ERR_MAP_IN_USE            = 12009   # 地图正在使用，无法删除
uint16 ERR_VERSION_CONFLICT      = 12010   # 版本冲突
```

| 错误码 | 常量名 | 说明 | 严重级别 |
|--------|--------|------|----------|
| 0 | OK | 成功 | — |
| 12001 | ERR_MAP_NOT_FOUND | 地图不存在 | MEDIUM |
| 12002 | ERR_MAP_LOAD_FAILED | 地图加载失败 | HIGH |
| 12003 | ERR_MAP_SAVE_FAILED | 地图保存失败 | MEDIUM |
| 12004 | ERR_MAP_DELETE_FAILED | 地图删除失败 | LOW |
| 12005 | ERR_STORAGE_FULL | 存储空间已满 | HIGH |
| 12006 | ERR_MAP_CORRUPTED | 地图文件损坏 | HIGH |
| 12007 | ERR_INVALID_MAP_TYPE | 非法地图类型 | LOW |
| 12008 | ERR_SYNC_FAILED | 云端同步失败 | MEDIUM |
| 12009 | ERR_MAP_IN_USE | 地图正在使用 | MEDIUM |
| 12010 | ERR_VERSION_CONFLICT | 版本冲突 | MEDIUM |

---

## 9. 安全约束

1. **删除确认**：RemoveMap 必须传 confirm=true，防止误删
2. **存储配额保护**：达到 max_storage_bytes 后拒绝新保存，除非删除旧地图
3. **完整性校验**：加载地图前校验 SHA256，损坏的地图拒绝加载并上报 HDS
4. **激活地图不可删除**：当前激活的地图必须先卸载才能删除
5. **云端同步验证**：下载的地图必须校验指纹，与云端记录不一致时拒绝

---

## 10. 包结构

```
mapmanager_msgs/        # 消息定义包
├── msg/
│   ├── MapManagerState.msg
│   ├── MapInfo.msg
│   ├── MapList.msg
│   └── Heartbeat.msg
├── srv/
│   ├── GetHealthStatus.srv
│   ├── SubmitMap.srv
│   ├── LoadMap.srv
│   ├── SaveMap.srv
│   ├── RemoveMap.srv
│   ├── GetMapList.srv
│   ├── GetMapInfo.srv
│   ├── SyncMapWithCloud.srv
│   └── GetActiveMap.srv
├── CMakeLists.txt
└── package.xml

mapmanager/             # 节点实现包
├── include/mapmanager/
│   ├── mapmanager_node.hpp
│   ├── map_registry.hpp
│   ├── storage_manager.hpp
│   ├── lifecycle_controller.hpp
│   ├── map_fusion_engine.hpp
│   ├── cloud_sync_agent.hpp
│   ├── quality_monitor.hpp
│   └── active_map_cache.hpp
├── src/
│   ├── mapmanager_node.cpp
│   ├── map_registry.cpp
│   ├── storage_manager.cpp
│   ├── lifecycle_controller.cpp
│   ├── map_fusion_engine.cpp
│   ├── cloud_sync_agent.cpp
│   ├── quality_monitor.cpp
│   ├── active_map_cache.cpp
│   └── main.cpp
├── test/
│   ├── test_map_registry.cpp
│   ├── test_storage_manager.cpp
│   ├── test_lifecycle.cpp
│   └── test_integration.cpp
├── config/
│   └── mapmanager_params.yaml
├── launch/
│   └── mapmanager.launch.py
├── CMakeLists.txt
└── package.xml
```

---

## 11. 关键性能指标 (KPI)

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 地图加载时间 | < 5s | 从调用 LoadMap 到激活完成 |
| 地图保存时间 | < 10s | 含压缩和校验 |
| 地图列表查询延迟 | < 100ms | 列出所有地图 |
| 存储配额利用率 | < 80% | 正常运行时 |
| 云端同步速度 | > 10MB/s | 地图上传/下载 |
| 地图完整性校验 | 100% | 加载前必须校验通过 |
| 激活地图切换停机 | < 2s | PnC 无地图可用的时间 |
| 内存占用 | < 1GB | 含激活地图缓存 |
| 地图版本保留 | >= 3 | 每个地图的历史版本数 |
| 损坏地图检测率 | 100% | 指纹校验不通过的地图不加载 |
