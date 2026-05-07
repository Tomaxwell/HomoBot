# Data Quality Filter 模块设计

## 1. 模块概述与定位

**模块名称**：Data Quality Filter（数据质量过滤器）

**定位**：Data Quality Filter 是端侧软件系统中**数据质量的实时评估与过滤中枢**，位于应用层。它以 1kHz 轻量级运行，持续评估传感器数据质量和 VLA 数据帧完整性，为 DR（Data Recorder）提供逐帧质量评分和冗余去重标记，从源头减少无效/冗余数据的存储和上传开销。

**核心职责**：

1. **传感器数据质量评分**：图像模糊检测、LiDAR 点云稀疏度评估、IMU 噪声水平监测
2. **VLA 数据帧完整性检查**：图像+关节状态+语言指令是否齐全，时间戳是否对齐
3. **冗余去重**：相似场景（同一位置/姿态）降采样，保留关键变化帧
4. **脏数据标记**：传感器故障期间的数据自动标记为不可用
5. **实时质量报告**：每帧输出质量评分，供 DR 分层存储决策
6. **阈值自适应**：根据数据分布动态调整质量阈值

**与相邻模块的边界**：

| 边界 | Data Quality Filter 负责 | 对方负责 |
|------|-------------------------|---------|
| Data Quality Filter ↔ DR | 输出逐帧质量评分、冗余标记、脏数据标记 | 数据录制、存储策略、按质量分层存储 |
| Data Quality Filter ↔ Perception | 读取图像/点云数据用于质量评估 | 传感器数据获取、多模态融合 |
| Data Quality Filter ↔ HAL_Sensor | 读取 IMU/力觉原始数据用于质量评估 | 传感器驱动、硬件状态上报 |
| Data Quality Filter ↔ HDS | 读取传感器健康状态 | 故障诊断、定级 |
| Data Quality Filter ↔ MC | 读取关节状态用于 VLA 帧完整性检查 | 运动控制、关节状态发布 |

---

## 2. 职责边界

**Data Quality Filter 不做的事情**（红线）：

- **不做数据采集** — 只评估已采集数据的质量，不主动采集
- **不做数据存储** — 只输出评分标记，不存储原始数据
- **不做数据上传** — 上传由 Data Uploader 负责
- **不做数据修复** — 只标记脏数据，不尝试修复
- **不做传感器标定** — 传感器内参/外参标定由 Perception 负责
- **不做决策生成** — 质量评分只供 DR 参考，不做业务决策

---

## 3. 状态机设计

Data Quality Filter 是**无状态流处理模块**，不存在复杂的运行状态机。模块运行状态如下：

| 状态 | 值 | 说明 |
|------|-----|------|
| `DQF_STATE_ACTIVE` | 0 | 正常运行，持续评估 |
| `DQF_STATE_DEGRADED` | 1 | 降级运行（部分评估器故障） |
| `DQF_STATE_FAULT` | 2 | 模块故障（所有评估器不可用） |

状态转换：
- ACTIVE → DEGRADED：部分评估器（如图像模糊检测）发生故障
- DEGRADED → ACTIVE：故障评估器恢复
- ACTIVE/DEGRADED → FAULT：核心评估框架崩溃
- FAULT → ACTIVE：模块重启恢复

---

## 4. ROS2 接口定义

### 4.1 消息定义（data_quality_msgs）

```
# data_quality_msgs/msg/DataQualityScore.msg
# 数据质量评分

uint8 SENSOR_IMAGE = 0
uint8 SENSOR_LIDAR = 1
uint8 SENSOR_IMU = 2
uint8 SENSOR_FORCE = 3
uint8 SENSOR_JOINT = 4
uint8 FRAME_VLA = 10

uint8 data_type                   # 数据类型
string data_id                    # 数据帧唯一ID
float32 overall_score             # 总体质量评分 0.0-1.0
float32 sharpness_score           # 图像清晰度评分（图像数据）
float32 density_score             # 点云密度评分（LiDAR数据）
float32 noise_score               # 噪声水平评分（IMU/力觉数据）
float32 completeness_score        # 完整性评分（VLA帧）
float32 timestamp_align_score     # 时间戳对齐评分
bool is_redundant                 # 是否冗余（相似帧）
bool is_dirty                     # 是否为脏数据
string[] dirty_reasons            # 脏数据原因列表
builtin_interfaces/Time stamp
```

```
# data_quality_msgs/msg/FrameQualityReport.msg
# VLA 数据帧质量报告

string frame_id
builtin_interfaces/Time timestamp

bool has_image_left
bool has_image_right
bool has_image_head
bool has_lidar
bool has_joint_state
bool has_language_instruction
bool timestamp_aligned

float32 overall_score
string[] missing_components
string[] quality_issues
```

```
# data_quality_msgs/msg/RedundancyFlag.msg
# 冗余标记

string data_id
bool is_redundant
float32 similarity_score          # 与上一关键帧的相似度
string reference_frame_id         # 参考关键帧ID
builtin_interfaces/Time stamp
```

```
# data_quality_msgs/msg/Heartbeat.msg
# 模块心跳

builtin_interfaces/Time stamp
uint8 state
uint32 error_code
uint32 frames_evaluated
uint32 frames_flagged_dirty
uint32 frames_flagged_redundant
```

### 4.2 服务定义（data_quality_msgs）

```
# data_quality_msgs/srv/EvaluateDataQuality.srv
# 手动触发数据质量评估

string file_path                  # 本地数据文件路径
uint8 data_type
---
bool success
DataQualityScore score
string message
```

```
# data_quality_msgs/srv/GetQualityThresholds.srv
# 获取当前质量阈值配置

---
bool success
float32 min_sharpness
float32 min_density
float32 max_noise
float32 min_completeness
float32 redundancy_threshold
```

### 4.3 Topic 汇总

| Topic | 类型 | 流向 | QoS | 频率 | 说明 |
|-------|------|------|-----|------|------|
| `/data_quality/score` | `DataQualityScore` | DQF → DR | Reliable + Volatile | 1kHz | 逐帧质量评分 |
| `/data_quality/frame_report` | `FrameQualityReport` | DQF → DR | Reliable + Volatile | 1kHz | VLA帧质量报告 |
| `/data_quality/redundancy_flag` | `RedundancyFlag` | DQF → DR | Reliable + Volatile | 1kHz | 冗余标记 |
| `/data_quality/heartbeat` | `Heartbeat` | DQF → EM/HDS | Reliable + Volatile | 1Hz | 模块心跳 |
| `/perception/image_raw` | `sensor_msgs/Image` | Perception → DQF | Best Effort | 30Hz | 图像质量评估输入 |
| `/perception/point_cloud` | `sensor_msgs/PointCloud2` | Perception → DQF | Best Effort | 10Hz | 点云质量评估输入 |
| `/hal_sensor/imu` | `sensor_msgs/Imu` | HAL_Sensor → DQF | Best Effort | 200Hz | IMU质量评估输入 |
| `/mc/joint_states` | `sensor_msgs/JointState` | MC → DQF | Best Effort | 1kHz | 关节状态完整性检查 |

### 4.4 Service 汇总

| Service | 类型 | 调用方 | 说明 |
|---------|------|--------|------|
| `/data_quality/evaluate` | `EvaluateDataQuality` | DR / TE | 手动触发评估 |
| `/data_quality/get_thresholds` | `GetQualityThresholds` | DR / Gateway | 查询阈值配置 |

---

## 5. 内部设计

### 5.1 节点结构

```
Data Quality Filter Node (1kHz cycle)
├── ImageQualityEvaluator（图像质量评估器）
│   ├── BlurDetector（拉普拉斯方差模糊检测）
│   ├── OverExposureDetector（过曝检测）
│   └── UnderExposureDetector（欠曝检测）
├── LidarQualityEvaluator（LiDAR质量评估器）
│   └── DensityAnalyzer（点云密度分析）
├── ImuQualityEvaluator（IMU质量评估器）
│   └── NoiseEstimator（噪声估计）
├── VlaFrameChecker（VLA帧完整性检查器）
│   ├── ComponentChecker（组件齐全检查）
│   └── TimestampAlignChecker（时间戳对齐检查）
├── RedundancyDetector（冗余检测器）
│   ├── FrameBuffer（帧缓冲区）
│   └── SimilarityScorer（相似度评分）
└── QualityAggregator（质量聚合器）
    ├── ScoreComposer（评分合成）
    └── ThresholdComparator（阈值比较）
```

### 5.2 关键流程

#### 5.2.1 逐帧质量评估流程（1kHz）

```
收到一帧数据（图像/点云/关节状态）
    ↓
根据数据类型分发到对应评估器
    ↓
Image: BlurDetector → OverExposureDetector → UnderExposureDetector
Lidar: DensityAnalyzer
IMU:   NoiseEstimator
Joint: N/A（只参与完整性检查）
    ↓
QualityAggregator 合成 overall_score
    ↓
RedundancyDetector 计算与上一关键帧的相似度
    ↓
ThresholdComparator 判断是否超过质量阈值 / 冗余阈值
    ↓
输出 DataQualityScore + RedundancyFlag
    ↓
DR 根据评分决定：保留 / 降采样 / 丢弃
```

#### 5.2.2 VLA 帧完整性检查流程

```
DR 组装 VLA 数据帧（图像 + 关节状态 + 语言指令）
    ↓
VlaFrameChecker 检查：
  - 所有必需组件是否存在
  - 各组件时间戳是否在 10ms 窗口内对齐
  - 图像分辨率是否符合要求
  - 关节状态是否包含所有下肢关节
    ↓
输出 FrameQualityReport
    ↓
完整性评分 < 阈值 → 标记为 dirty，建议丢弃该帧
```

### 5.3 图像模糊检测算法

采用拉普拉斯方差法（Laplacian Variance）：

```
score = variance(cv2.Laplacian(image, CV_64F))
sharpness_score = min(score / threshold, 1.0)
```

阈值根据场景自适应调整（室内/室外/光照条件）。

### 5.4 冗余检测算法

采用帧间差异哈希（dHash）+ 姿态向量余弦相似度：

```
image_similarity = 1.0 - hamming_distance(dhash(current), dhash(last_keyframe)) / 64
pose_similarity = cosine_similarity(joint_poses(current), joint_poses(last_keyframe))
overall_similarity = 0.6 * image_similarity + 0.4 * pose_similarity
is_redundant = overall_similarity > redundancy_threshold
```

当 `is_redundant == true` 时，该帧建议降采样（每 N 帧保留 1 帧）。

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 交互内容 | 方向 | 接口类型 |
|------|---------|------|---------|
| DR | 质量评分、冗余标记、脏数据标记 | DQF → DR | Topic |
| Perception | 图像、点云原始数据 | DQF ← Perception | Topic |
| HAL_Sensor | IMU、力觉原始数据 | DQF ← HAL_Sensor | Topic |
| MC | 关节状态 | DQF ← MC | Topic |
| HDS | 传感器健康状态 | DQF ← HDS | Topic |
| TE | 语言指令（VLA帧完整性检查） | DQF ← TE | Topic |
| Setting | 质量阈值配置 | DQF ← Setting | 参数读取 |
| EM | 进程管理 | DQF ↔ EM | 心跳 + 控制 |

### 6.2 与 DR 的集成方式

Data Quality Filter 作为 DR 的**协作节点**运行（非子模块），通过 Topic 异步通信：

```
DR (录制流程)              Data Quality Filter (评估流程)
     │                              │
     │ 组装VLA帧 ──────────────────►│ 帧完整性检查
     │                              │
     │ ◄──────── FrameQualityReport │
     │                              │
     │ 决定是否落盘 ◄───────────────│ overall_score
     │                              │
     │ ◄──────── RedundancyFlag     │ 冗余标记
     │                              │
     │ 决定是否降采样 ◄─────────────│
```

DR 的落盘决策逻辑：
- `overall_score >= 0.8` → 高质量，长期保留
- `0.5 <= overall_score < 0.8` → 中等质量，循环覆盖
- `overall_score < 0.5` → 低质量，立即丢弃
- `is_dirty == true` → 强制丢弃
- `is_redundant == true` → 降采样保留（每 5 帧保留 1 帧）

---

## 7. 关键参数与配置

### 7.1 参数文件 `data_quality_params.yaml`

```yaml
data_quality_filter:
  # 质量阈值
  thresholds:
    min_sharpness: 100.0           # 拉普拉斯方差最小值
    min_density: 1000              # 每帧最少点数
    max_noise: 0.05                # 最大噪声标准差 (m/s^2)
    min_completeness: 0.9          # VLA帧最小完整性
    redundancy_threshold: 0.95     # 冗余判定相似度阈值

  # 评估器开关
  evaluators:
    image_quality: true
    lidar_quality: true
    imu_quality: true
    vla_completeness: true
    redundancy_detection: true

  # 冗余检测
  redundancy:
    keyframe_interval_max: 30      # 最大关键帧间隔（帧数）
    similarity_weight_image: 0.6   # 图像相似度权重
    similarity_weight_pose: 0.4    # 姿态相似度权重

  # 性能
  max_eval_latency_ms: 1.0         # 单帧评估最大延迟
  buffer_size: 100                 # 帧缓冲区大小
```

---

## 8. 错误码定义

Data Quality Filter 错误码范围：**9501-9520**（避免与 Perception 9001-9019 冲突）

| 码 | 常量 | 说明 | 级别 |
|----|------|------|------|
| 9501 | DQF_ERR_IMAGE_EVAL_FAIL | 图像质量评估失败 | MEDIUM |
| 9502 | DQF_ERR_LIDAR_EVAL_FAIL | LiDAR质量评估失败 | MEDIUM |
| 9503 | DQF_ERR_IMU_EVAL_FAIL | IMU质量评估失败 | LOW |
| 9504 | DQF_ERR_VLA_CHECK_FAIL | VLA帧完整性检查失败 | MEDIUM |
| 9505 | DQF_ERR_REDUNDANCY_DETECT_FAIL | 冗余检测失败 | LOW |
| 9506 | DQF_ERR_BUFFER_OVERFLOW | 帧缓冲区溢出 | MEDIUM |
| 9507 | DQF_ERR_EVAL_TIMEOUT | 单帧评估超时 | HIGH |
| 9508 | DQF_ERR_SENSOR_DATA_MISSING | 传感器数据缺失 | HIGH |
| 9509 | DQF_ERR_TIMESTAMP_MISALIGN | 时间戳严重不对齐 | HIGH |
| 9510 | DQF_ERR_THRESHOLD_INVALID | 阈值配置非法 | MEDIUM |

---

## 9. 包结构

```
data_quality_filter/
├── include/data_quality_filter/
│   ├── data_quality_filter_node.hpp
│   ├── image_quality_evaluator.hpp
│   ├── lidar_quality_evaluator.hpp
│   ├── imu_quality_evaluator.hpp
│   ├── vla_frame_checker.hpp
│   ├── redundancy_detector.hpp
│   └── quality_aggregator.hpp
├── src/
│   ├── data_quality_filter_node.cpp
│   ├── image_quality_evaluator.cpp
│   ├── lidar_quality_evaluator.cpp
│   ├── imu_quality_evaluator.cpp
│   ├── vla_frame_checker.cpp
│   ├── redundancy_detector.cpp
│   └── quality_aggregator.cpp
├── config/
│   └── data_quality_params.yaml
├── launch/
│   └── data_quality_filter.launch.py
├── CMakeLists.txt
└── package.xml

data_quality_msgs/
├── msg/
│   ├── DataQualityScore.msg
│   ├── FrameQualityReport.msg
│   ├── RedundancyFlag.msg
│   └── Heartbeat.msg
├── srv/
│   ├── EvaluateDataQuality.srv
│   └── GetQualityThresholds.srv
├── CMakeLists.txt
└── package.xml
```

---

## 10. 安全约束

1. **实时性保证**：单帧评估延迟必须 < 1ms，超时则跳过该帧评估，不阻塞 DR
2. **只读访问**：评估器只读取传感器数据，不修改任何 Topic 内容
3. **降级 graceful**：单个评估器故障时，其他评估器继续工作，模块进入 DEGRADED 状态
4. **无状态依赖**：评估不依赖历史状态，每帧独立评估（冗余检测除外，缓冲区有限）
5. **阈值保护**：质量阈值有最小值/最大值限制，防止配置错误导致全部数据被丢弃
6. **缓冲区限制**：帧缓冲区大小固定，溢出时丢弃最旧帧，防止内存泄漏
