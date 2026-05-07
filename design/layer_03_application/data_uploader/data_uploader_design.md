# Data Uploader 模块设计

## 1. 模块概述与定位

**模块名称**：Data Uploader（数据上传器）

**定位**：Data Uploader 是端侧软件系统中**端云数据资产的专用上传通道**，位于应用层。它是 DR（Data Recorder）与云端物理AI数据服务平台（觅蜂/coScene）之间的数据流转中枢，负责将端侧采集、筛选后的数据资产高效、安全、可靠地上传至云端。

**与 Gateway 的关系**：Data Uploader 是 Gateway 内部的**数据资产上传子模块**，不独立连接云端。所有云端通信必须通过 Gateway 的统一出口转发。Gateway 负责控制指令/状态的通信，Data Uploader 负责数据资产的上传，两者在 Gateway 内部通过带宽隔离机制共享物理连接，但在逻辑上完全隔离。

**核心职责**：

1. **上传任务调度**：管理上传队列，支持优先级调度（故障数据 > 训练数据 > 日志数据）
2. **断点续传**：大文件分片上传，支持网络中断后从断点恢复
3. **智能压缩**：视频数据 H.265/AV1 编码，点云/关节数据 gzip/zstd 压缩
4. **加密与脱敏**：TLS 传输加密，敏感区域（人脸/车牌）自动模糊脱敏
5. **带宽自适应**：监测网络质量，动态调整上传速率，确保不抢占控制指令带宽
6. **上传状态追踪**：记录每个数据包的上传进度、成功/失败状态、重试次数
7. **闲时上传**：支持配置上传时段（如夜间闲时），降低对业务运行的影响

**与相邻模块的边界**：

| 边界 | Data Uploader 负责 | 对方负责 |
|------|-------------------|---------|
| Data Uploader ↔ DR | 读取本地数据文件，获取数据资产元数据 | 数据录制、存储、质量评分 |
| Data Uploader ↔ Gateway | 请求网络带宽配额；复用 TLS 连接（逻辑隔离） | 云端连接管理、认证、控制指令转发 |
| Data Uploader ↔ Setting | 读取上传策略、带宽限制、上传时段配置 | 参数持久化与验证 |
| Data Uploader ↔ HDS | 上报上传模块健康状态 | 故障诊断与定级 |
| Data Uploader ↔ 云端 | 数据资产的上传协议、格式转换、校验 | 云端数据平台接收、存储、处理 |

---

## 2. 职责边界

**Data Uploader 不做的事情**（红线）：

- **不做数据采集** — 只负责上传，数据录制是 DR 的职责
- **不做数据质量评估** — 只上传已被 DR/Data Quality Filter 标记为可用的数据
- **不做控制指令传输** — 控制指令由 Gateway 负责，两者带宽隔离
- **不做云端业务逻辑** — 只负责数据传输，云端的数据清洗/标注/训练由云端平台完成
- **不修改原始数据** — 压缩/脱敏只作用于上传副本，本地原始数据只读
- **不直接访问传感器** — 不订阅传感器 Topic，只读取 DR 生成的数据文件

---

## 3. 状态机设计

### 3.1 上传任务状态枚举

| 状态 | 值 | 说明 |
|------|-----|------|
| `DU_IDLE` | 0 | 空闲，无上传任务 |
| `DU_QUEUING` | 1 | 任务入队，等待调度 |
| `DU_COMPRESSING` | 2 | 正在压缩预处理 |
| `DU_UPLOADING` | 3 | 正在上传 |
| `DU_PAUSED` | 4 | 暂停（带宽不足或超出上传时段） |
| `DU_VERIFYING` | 5 | 上传完成，正在校验完整性 |
| `DU_COMPLETED` | 6 | 上传成功，云端已确认 |
| `DU_FAILED` | 7 | 上传失败（超过重试次数） |
| `DU_CANCELLED` | 8 | 任务被取消 |

### 3.2 模块运行状态机

| 状态 | 值 | 说明 |
|------|-----|------|
| `DU_STATE_STANDBY` | 0 | 待机，等待上传任务 |
| `DU_STATE_ACTIVE` | 1 | 活跃，正在处理上传队列 |
| `DU_STATE_BANDWIDTH_LIMITED` | 2 | 带宽受限，降速运行 |
| `DU_STATE_OFFLINE` | 3 | 网络不可用，任务挂起 |
| `DU_STATE_FAULT` | 4 | 模块故障（存储读取失败/压缩引擎崩溃） |

### 3.3 状态转换图

```
                              queue_not_empty
                    ┌──────────────────────────────────┐
                    │                                  │
              ┌─────┴────────┐                        ▼
        ┌──►  │  DU_STANDBY  │──────────────────►┌──────────────┐
        │     │      0       │                   │  DU_ACTIVE   │
        │     └──────────────┘                   │      1       │
        │           ▲                            └──────┬───────┘
        │           │                                   │
        │    queue_empty                                │ bw_limited
        │           │                                   ▼
        │           │                            ┌──────────────┐
        │           │                            │ DU_BANDWIDTH │
        │           │                            │   LIMITED    │
        │           │                            │      2       │
        │           │                            └──────┬───────┘
        │           │                                   │ bw_recovered
        │           │                                   │
        │    ┌──────┘                                   ▼
        │    ▼                                    ┌──────────────┐
        │  ┌──────────────┐   network_lost       │   DU_OFFLINE │
        │  │  DU_FAULT    │◄────────────────────│      3       │
        │  │      4       │                     └──────┬───────┘
        │  └──────────────┘                            │ network_back
        │           ▲                                  │
        │           │ recover                          ▼
        │           │                            ┌──────────────┐
        │           └────────────────────────────│  DU_STANDBY  │
        │                                        │      0       │
        │                                        └──────────────┘
        │
        └─── fault_occurred
```

---

## 4. ROS2 接口定义

### 4.1 消息定义（data_uploader_msgs）

```
# data_uploader_msgs/msg/UploadStatus.msg
# 上传任务状态

string task_id                    # 上传任务唯一ID
string data_asset_id              # 对应数据资产ID
uint8 state                       # 上传状态（见状态机枚举）
float32 progress_percent          # 上传进度 0.0-100.0
uint64 bytes_uploaded             # 已上传字节数
uint64 bytes_total                # 总字节数
float32 upload_speed_bps          # 当前上传速度 (bytes/s)
uint32 retry_count                # 已重试次数
string error_message              # 失败时的错误信息
builtin_interfaces/Time stamp
```

```
# data_uploader_msgs/msg/DataAssetInfo.msg
# 数据资产元数据

string asset_id                   # 资产唯一ID
string asset_type                 # "vla_training" / "failure_blackbox" / "operation_log" / "sensor_raw"
string source_module              # 数据来源模块（DR / HDS / MC等）
string file_path                  # 本地文件路径
uint64 file_size_bytes            # 文件大小
string checksum_sha256            # SHA256校验值
string[] tags                     # 标签（如 ["failure", "auto_capture", "high_quality"]）
float32 quality_score             # 数据质量评分 0.0-1.0
builtin_interfaces/Time created_at
builtin_interfaces/Time stamp
```

```
# data_uploader_msgs/msg/UploadPriority.msg
# 上传优先级（供队列排序使用）

uint8 PRIORITY_CRITICAL = 0       # 故障数据，立即上传
uint8 PRIORITY_HIGH = 1           # 训练数据，优先上传
uint8 PRIORITY_NORMAL = 2         # 常规日志，闲时上传
uint8 PRIORITY_LOW = 3            # 诊断数据，最低优先级

uint8 priority
```

```
# data_uploader_msgs/msg/BandwidthStatus.msg
# 带宽状态

float32 allocated_bandwidth_bps   # 分配到的带宽
float32 used_bandwidth_bps        # 已使用带宽
float32 available_bandwidth_bps   # 可用带宽
bool is_limited                   # 是否被限速
string limit_reason               # 限速原因
```

### 4.2 服务定义（data_uploader_msgs）

```
# data_uploader_msgs/srv/TriggerUpload.srv
# 手动触发上传指定数据资产

string asset_id
uint8 priority
bool compressed                   # 是否先压缩再上传
---
bool success
string task_id
string message
```

```
# data_uploader_msgs/srv/GetUploadQueue.srv
# 查询上传队列状态

---
bool success
data_uploader_msgs/UploadStatus[] tasks
uint32 total_pending
uint32 total_uploading
uint32 total_completed_today
uint64 total_bytes_uploaded_today
```

```
# data_uploader_msgs/srv/CancelUpload.srv
# 取消上传任务

string task_id
---
bool success
string message
```

### 4.3 Action 定义（data_uploader_msgs）

```
# data_uploader_msgs/action/UploadDataset.action
# 大批量数据集上传（含进度反馈）

string dataset_id
string dataset_path
string[] asset_ids
bool delete_after_upload          # 上传成功后是否删除本地文件
---
bool success
uint64 total_bytes_uploaded
string[] failed_asset_ids
---
string current_asset_id
float32 overall_progress
float32 current_asset_progress
uint64 bytes_uploaded_so_far
```

### 4.4 Topic 汇总

| Topic | 类型 | 流向 | QoS | 频率 | 说明 |
|-------|------|------|-----|------|------|
| `/data_uploader/upload_status` | `UploadStatus` | Data Uploader → ALL | Reliable + Volatile | 事件驱动 | 上传进度实时广播 |
| `/data_uploader/bandwidth_status` | `BandwidthStatus` | Data Uploader → ALL | Reliable + Volatile | 1Hz | 带宽使用状态 |
| `/data_uploader/heartbeat` | `data_uploader_msgs/Heartbeat` | Data Uploader → EM/HDS | Reliable + Volatile | 1Hz | 模块心跳 |

### 4.5 Service 汇总

| Service | 类型 | 调用方 | 说明 |
|---------|------|--------|------|
| `/data_uploader/trigger_upload` | `TriggerUpload` | TE / Gateway / 人工 | 手动触发上传 |
| `/data_uploader/get_upload_queue` | `GetUploadQueue` | TE / Gateway | 查询队列状态 |
| `/data_uploader/cancel_upload` | `CancelUpload` | TE / Gateway | 取消上传任务 |

---

## 5. 内部设计

### 5.1 节点结构

```
Data Uploader Node
├── UploadManager（上传调度器）
│   ├── UploadQueue（优先级队列）
│   ├── BandwidthLimiter（带宽限制器）
│   └── ScheduleEngine（调度引擎）
├── CompressionEngine（压缩引擎）
│   ├── VideoEncoder（H.265/AV1）
│   ├── PointCloudCompressor（zstd）
│   └── GenericCompressor（gzip）
├── UploadEngine（上传引擎）
│   ├── ChunkUploader（分片上传）
│   ├── ResumeManager（断点管理）
│   └── RetryHandler（重试处理器）
├── EncryptionModule（加密模块）
│   ├── TLSChannel（TLS传输通道）
│   └── AnonymizationFilter（脱敏过滤器）
└── StorageReader（存储读取器）
    ├── FileScanner（文件扫描）
    └── MetadataParser（元数据解析）
```

### 5.2 关键流程

#### 5.2.1 数据上传主流程

```
DR 完成数据录制 → 生成 DataAssetInfo → 发送给 Data Uploader
    ↓
UploadManager 将任务入队（按优先级排序）
    ↓
ScheduleEngine 检查带宽配额 → 分配上传窗口
    ↓
StorageReader 读取本地数据文件
    ↓
CompressionEngine 按需压缩（视频→H.265，点云→zstd）
    ↓
AnonymizationFilter 脱敏处理（人脸/车牌模糊）
    ↓
ChunkUploader 分片上传至云端
    ↓
ResumeManager 每片上传成功后记录断点
    ↓
全部片上传完成 → 云端校验 SHA256
    ↓
校验通过 → 标记 COMPLETED → 通知 DR 可清理本地副本
    ↓
校验失败 → 自动重传异常片 → 超过阈值标记 FAILED
```

#### 5.2.2 带宽自适应流程

```
Gateway 报告当前控制指令带宽占用
    ↓
BandwidthLimiter 计算 Data Uploader 可用带宽
    ↓
可用带宽 < 阈值 → 降速/暂停低优先级任务
    ↓
可用带宽充裕 → 恢复/加速上传
    ↓
网络中断 → 所有任务 PAUSED → 断点保存 → 网络恢复后自动续传
```

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 交互内容 | 方向 | 接口类型 |
|------|---------|------|---------|
| DR | 数据资产元数据、本地文件路径 | Data Uploader ← DR | Topic + 文件系统 |
| Gateway | 带宽配额申请；通过 Gateway 云端出口转发数据 | Data Uploader ↔ Gateway | Service + 共享连接 |
| Setting | 上传策略、带宽限制、时段配置 | Data Uploader ← Setting | 参数读取 |
| HDS | 上传模块健康状态 | Data Uploader → HDS | Topic |
| EM | 进程生命周期管理 | Data Uploader ↔ EM | 心跳 + 控制 |

### 6.2 与 Gateway 的带宽隔离机制

Data Uploader 作为 Gateway 内部子模块运行，所有云端通信经 Gateway 统一出口转发。Gateway 内部逻辑带宽隔离：

```
总上行带宽
    ├── 控制指令预留（固定30%，最低保证）
    ├── Data Uploader 动态配额（剩余带宽的70%，可调整）
    └── 突发缓冲区（10%，供故障数据紧急上传）
```

Gateway 作为带宽仲裁者，Data Uploader 每次上传前申请配额，Gateway 根据当前控制指令流量动态分配。

---

## 7. 关键参数与配置

### 7.1 参数文件 `data_uploader_params.yaml`

```yaml
data_uploader:
  # 上传策略
  upload_mode: "adaptive"          # adaptive / immediate / scheduled
  scheduled_upload_window:
    start: "02:00"                 # 闲时上传开始时间
    end: "06:00"                   # 闲时上传结束时间
  max_concurrent_uploads: 3        # 最大并发上传数
  max_retry_count: 5               # 最大重试次数
  retry_backoff_base_sec: 10       # 退避基数（秒）

  # 带宽管理
  bandwidth:
    max_upload_rate_mbps: 10.0     # 最大上传速率
    control_reserved_percent: 30   # 控制指令预留带宽比例
    adaptive_min_rate_mbps: 0.5    # 最低保证上传速率

  # 压缩策略
  compression:
    video_codec: "h265"            # h264 / h265 / av1
    video_quality: 28              # CRF值（越小质量越高）
    pointcloud_codec: "zstd"       # zstd / gzip
    pointcloud_level: 3            # 压缩等级
    enable_compression: true       # 是否启用压缩

  # 脱敏策略
  anonymization:
    enabled: true
    blur_faces: true
    blur_license_plates: true
    blur_regions: []               # 自定义敏感区域（边界框列表）

  # 存储
  storage:
    temp_compression_path: "/tmp/data_uploader/compress"
    max_temp_size_gb: 50
    delete_after_upload: false     # 上传成功后是否删除本地文件
```

---

## 8. 错误码定义

Data Uploader 错误码范围：**7001-7020**

| 码 | 常量 | 说明 | 级别 |
|----|------|------|------|
| 7001 | DU_ERR_STORAGE_READ_FAIL | 本地存储读取失败 | HIGH |
| 7002 | DU_ERR_COMPRESSION_FAIL | 压缩引擎失败 | MEDIUM |
| 7003 | DU_ERR_NETWORK_UNREACHABLE | 网络不可达 | HIGH |
| 7004 | DU_ERR_UPLOAD_TIMEOUT | 上传超时 | MEDIUM |
| 7005 | DU_ERR_CHECKSUM_MISMATCH | 云端校验失败 | HIGH |
| 7006 | DU_ERR_QUOTA_EXCEEDED | 超出云端存储配额 | MEDIUM |
| 7007 | DU_ERR_ENCRYPTION_FAIL | 加密/脱敏失败 | HIGH |
| 7008 | DU_ERR_FILE_NOT_FOUND | 本地数据文件不存在 | HIGH |
| 7009 | DU_ERR_BANDWIDTH_DENIED | Gateway拒绝带宽申请 | LOW |
| 7010 | DU_ERR_TEMP_STORAGE_FULL | 临时压缩空间不足 | MEDIUM |
| 7011 | DU_ERR_CLOUD_AUTH_FAIL | 云端认证失败 | CRITICAL |
| 7012 | DU_ERR_INVALID_ASSET_TYPE | 不支持的数据资产类型 | MEDIUM |

---

## 9. 包结构

```
data_uploader/
├── include/data_uploader/
│   ├── upload_manager.hpp
│   ├── upload_queue.hpp
│   ├── bandwidth_limiter.hpp
│   ├── compression_engine.hpp
│   ├── chunk_uploader.hpp
│   ├── resume_manager.hpp
│   ├── retry_handler.hpp
│   ├── encryption_module.hpp
│   └── storage_reader.hpp
├── src/
│   ├── data_uploader_node.cpp
│   ├── upload_manager.cpp
│   ├── upload_queue.cpp
│   ├── bandwidth_limiter.cpp
│   ├── compression_engine.cpp
│   ├── chunk_uploader.cpp
│   ├── resume_manager.cpp
│   ├── retry_handler.cpp
│   ├── encryption_module.cpp
│   └── storage_reader.cpp
├── config/
│   └── data_uploader_params.yaml
├── launch/
│   └── data_uploader.launch.py
├── CMakeLists.txt
└── package.xml

data_uploader_msgs/
├── msg/
│   ├── UploadStatus.msg
│   ├── DataAssetInfo.msg
│   ├── UploadPriority.msg
│   ├── BandwidthStatus.msg
│   └── Heartbeat.msg
├── srv/
│   ├── TriggerUpload.srv
│   ├── GetUploadQueue.srv
│   └── CancelUpload.srv
├── action/
│   └── UploadDataset.action
├── CMakeLists.txt
└── package.xml
```

---

## 10. 安全约束

1. **带宽隔离**：Data Uploader 任何时候不得占用超过分配配额的带宽，控制指令优先绝对保证
2. **数据只读**：上传过程中只读取本地数据文件副本，不得修改 DR 管理的原始数据
3. **传输加密**：所有数据上传必须经过 TLS 加密，禁止明文传输
4. **脱敏强制**：包含人脸、车牌等敏感信息的数据必须经过脱敏处理才能上传
5. **断点安全**：断点信息必须加密存储，防止数据包被恶意拼接
6. **存储清理**：临时压缩文件必须在上传完成后或模块退出时自动清理
7. **配额控制**：严格遵守云端存储配额，超出配额时自动暂停非紧急上传
