# Data Recorder 模块设计

## 1. 模块概述与定位

**模块名称**：Data Recorder（数据采集）

**定位**：DR 是端侧软件系统中的**数据采集与记录中心**，位于应用层。它负责在机器人运行过程中按需或按策略记录各类数据（传感器原始数据、感知结果、运动指令、状态转换事件、任务执行日志等），支持 VLA 训练数据管理、故障复现分析、性能评估等场景。DR 是机器人"黑匣子"和"训练数据工厂"的结合体。

**核心职责**：

1. **按需录制**：接收来自 TE/Gateway 的录制启动/停止指令，记录指定 Topic 数据
2. **自动录制策略**：基于事件触发自动录制（如故障发生前后 30s 自动保存）
3. **VLA 训练数据管理**：按 VLA 格式组织录制数据（图像 + 动作标签 + 语言指令）
4. **数据压缩与存储**：实时压缩（H.264/gzip）并写入磁盘，支持循环录制
5. **数据检索与导出**：按时间、事件、任务 ID 检索录制片段，支持导出
6. **存储管理**：监控存储空间，自动清理过期数据
7. **数据标注**：支持录制时附加元数据标注（任务类型、场景标签、操作者注释）

**与相邻模块的边界**：

| 边界 | DR 负责 | 对方负责 |
|------|--------|---------|
| DR ↔ ALL | 订阅并记录各模块的 Topic 数据 | 各模块产生数据 |
| DR ↔ TE | 接收录制启停指令；按任务组织数据 | 任务调度 |
| DR ↔ Gateway | 接收云端录制请求；上传数据到云端 | 云端通信 |
| DR ↔ SM | 订阅状态转换事件用于自动触发 | 状态机决策 |
| DR ↔ HDS | 接收故障事件用于自动保存黑匣子 | 故障诊断 |
| DR ↔ Setting | 读取录制策略、存储路径配置 | 参数持久化 |
| DR ↔ Agent | 接收语言指令用于 VLA 数据标注 | VLA 决策 |

---

## 2. 职责边界

**DR 不做的事情**（红线）：

- **不做实时业务逻辑** — 只记录数据，不基于记录数据做决策
- **不做数据训练** — 数据仅存储和传输，模型训练在云端完成
- **不做数据修改** — 录制数据只读，不提供编辑修改功能
- **不直接连接云端** — 数据上传通过 Gateway
- **不做故障诊断** — 只记录原始数据供 HDS 分析
- **不影响系统性能** — 录制过程不能阻塞被记录模块的正常运行

---

## 3. 状态机设计

### 3.1 录制状态枚举

| 状态 | 值 | 说明 |
|------|-----|------|
| `DR_IDLE` | 0 | 空闲，未录制 |
| `DR_ARMED` | 1 | 已就绪，等待触发条件 |
| `DR_RECORDING` | 2 | 正在录制 |
| `DR_PAUSED` | 3 | 已暂停（可恢复） |
| `DR_SAVING` | 4 | 正在保存/压缩录制文件 |
| `DR_EXPORTING` | 5 | 正在导出数据 |
| `DR_ERROR` | 6 | 录制出错（存储满/写入失败） |

### 3.2 状态转换图

```
                        ┌───────────────────────────────────────────────┐
                        │                                               │
                  ┌─────┴──────┐   arm / enable     ┌──────────────────┴───┐
            ┌──►  │   DR_IDLE  │───────────────────►│      DR_ARMED        │
            │     │     0      │                    │         1            │
            │     └────────────┘                    └──────────┬───────────┘
            │           ▲                                      │
            │           │         manual_start / auto_trigger  │
            │           │    ┌─────────────────────────────────┘
            │           │    ▼
            │           │  ┌──────────────────┐   pause      ┌───────────┐
            │           │  │   DR_RECORDING   │─────────────►│ DR_PAUSED │
            │           │  │        2         │              │     3     │
            │           │  └────────┬─────────┘              └─────┬─────┘
            │           │           │ stop / complete                │ resume
            │           │    ┌──────┘                              │
            │           │    ▼                                     │
            │           │  ┌──────────────────┐◄───────────────────┘
            │           └──│     DR_SAVING    │
            │              │        4         │
            │              └────────┬─────────┘
            │                       │ save_complete
            │                       ▼
            │              ┌──────────────────┐
            │              │    DR_EXPORTING  │◄──── export_request
            │              │        5         │
            │              └────────┬─────────┘
            │                       │ export_complete
            │                       │
            │              ┌────────▼────────┐
            └─────────────│     DR_ERROR    │◄──── storage_full / write_fail
                           │        6        │
                           └─────────────────┘
```

### 3.3 状态转换表

| 当前状态 | 目标状态 | 触发条件 | 优先级 | 说明 |
|----------|----------|----------|--------|------|
| IDLE | ARMED | 收到 arm 指令 | 60 | 准备录制 |
| IDLE | RECORDING | 直接收到 start 指令 | 60 | 立即录制 |
| ARMED | RECORDING | 手动开始或自动触发 | 60 | 开始录制 |
| ARMED | IDLE | disarm 指令 | 50 | 解除就绪 |
| RECORDING | PAUSED | pause 指令 | 70 | 暂停录制 |
| PAUSED | RECORDING | resume 指令 | 70 | 恢复录制 |
| PAUSED | IDLE | stop 指令 | 70 | 停止（丢弃） |
| RECORDING | SAVING | stop 指令或录制完成 | 60 | 开始保存 |
| SAVING | IDLE | 保存完成 | 60 | 回到空闲 |
| SAVING | EXPORTING | 导出请求 | 60 | 导出数据 |
| EXPORTING | IDLE | 导出完成 | 60 | 回到空闲 |
| * | ERROR | 存储满或写入失败 | 80 | 录制错误 |
| ERROR | IDLE | 错误处理完成/人工复位 | 60 | 错误恢复 |

### 3.4 状态转换约束

1. **存储阈值**：磁盘使用率 > 85% → 自动停止新录制，标记 ERROR
2. **循环录制**：支持循环缓冲区模式， oldest 数据自动覆盖
3. **预录缓冲**：ARMED 状态下维护 30s 环形缓冲，触发时可保存触发前数据
4. **并发限制**：最多同时录制 3 个独立会话（不同任务/场景）
5. **状态变更时**发布 `/dr/recorder_state` Topic

---

## 4. ROS2 接口定义

### 4.1 消息定义 (msg)

```
# dr_msgs/msg/RecorderState.msg
# 录制状态广播

uint8 state
uint8 prev_state
builtin_interfaces/Time state_changed_at
string session_id           # 当前录制会话ID
float32 record_duration_sec # 已录制时长（秒）
uint64 bytes_recorded       # 已录制字节数
float32 storage_usage_percent  # 存储使用率
```

```
# dr_msgs/msg/RecordingSession.msg
# 录制会话信息

string session_id
string name
string description
builtin_interfaces/Time started_at
builtin_interfaces/Time stopped_at
float32 duration_sec
uint64 size_bytes
string[] topics_recorded    # 录制的Topic列表
string trigger_type         # 触发类型（"manual", "auto_fault", "auto_task", "scheduled"）
string task_id              # 关联的任务ID
string[] tags               # 标签（用于检索）
bool is_vla_data            # 是否为VLA训练数据
```

```
# dr_msgs/msg/VlaDataFrame.msg
# VLA训练数据帧

builtin_interfaces/Time stamp
sensor_msgs/CompressedImage rgb_image      # RGB图像
sensor_msgs/CompressedImage depth_image    # 深度图像（可选）
float64[] joint_positions   # 当前关节角度（作为动作标签）
float64[] joint_velocities  # 当前关节速度
string language_instruction # 语言指令（如"走到桌子前面"）
string task_phase           # 任务阶段（"start", "middle", "end"）
bool is_keyframe            # 是否关键帧
```

```
# dr_msgs/msg/Heartbeat.msg
# DR 心跳

builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message
float32 storage_usage_percent
```

### 4.2 服务定义 (srv)

```
# dr_msgs/srv/GetHealthStatus.srv

# Request（空）
---
# Response
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
float32 storage_usage_percent
```

```
# dr_msgs/srv/StartRecording.srv
# 开始录制

string name                 # 录制名称
string description
string[] topics             # 要录制的Topic列表（空=录制全部）
string trigger_type         # "manual", "auto_fault", "auto_task"
string task_id              # 关联任务ID
string[] tags               # 标签
bool is_vla_mode            # 是否VLA模式
float32 max_duration_sec    # 最大录制时长（0=无限制）
---
# Response
bool success
uint16 error_code
string message
string session_id
```

```
# dr_msgs/srv/StopRecording.srv
# 停止录制

string session_id
---
# Response
bool success
uint16 error_code
string message
RecordingSession session_info
```

```
# dr_msgs/srv/GetRecordingList.srv
# 列出录制会话

string tag_filter           # 标签过滤（空=全部）
builtin_interfaces/Time start_time  # 时间范围开始
builtin_interfaces/Time end_time    # 时间范围结束
---
# Response
bool success
RecordingSession[] sessions
uint32 count
```

```
# dr_msgs/srv/ExportRecording.srv
# 导出录制数据

string session_id
string export_path          # 导出路径
string format               # "rosbag2", "mcap", "hdf5"
---
# Response
bool success
uint16 error_code
string message
uint64 bytes_exported
```

```
# dr_msgs/srv/DeleteRecording.srv
# 删除录制

string session_id
bool confirm
---
# Response
bool success
uint16 error_code
string message
```

```
# dr_msgs/srv/TagRecording.srv
# 为录制添加标签

string session_id
string[] tags_to_add
---
# Response
bool success
uint16 error_code
string message
```

### 4.3 接口汇总表

#### Topics（DR 订阅的内部 Topics——示例）

| 名称 | 类型 | 来源 | 说明 |
|------|------|------|------|
| `/camera/image_raw` | `sensor_msgs/msg/Image` | HAL_Sensor | 原始图像 |
| `/lidar/point_cloud` | `sensor_msgs/msg/PointCloud2` | HAL_Sensor | 原始点云 |
| `/sm/robot_state` | `sm_msgs/msg/RobotState` | SM | 机器人状态 |
| `/sm/transition_event` | `sm_msgs/msg/TransitionEvent` | SM | 状态转换事件 |
| `/te/task_state` | `te_msgs/msg/TaskState` | TE | 任务状态 |
| `/mc/mc_state` | `mc_msgs/msg/McState` | MC | 运动状态 |
| `/pnc/navigation_state` | `pnc_msgs/msg/NavigationState` | PnC | 导航状态 |
| `/perception/result` | `perception_msgs/msg/PerceptionResult` | Perception | 感知结果 |
| `/hds/health_report` | `hds_msgs/msg/HealthReport` | HDS | 健康报告 |

#### Topics（DR 发布）

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/dr/recorder_state` | `dr_msgs/msg/RecorderState` | DR → ALL | Reliable + Transient Local + Depth 1 | 事件驱动 | 录制状态广播 |
| `/dr/vla_data_frame` | `dr_msgs/msg/VlaDataFrame` | DR → ALL | Reliable + Volatile + Depth 10 | 10 Hz | VLA数据帧（VLA模式时） |
| `/dr/heartbeat` | `dr_msgs/msg/Heartbeat` | DR → EM/HDS | Reliable + Volatile + Depth 1 | 1 Hz | 心跳 |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/dr/get_health_status` | `dr_msgs/srv/GetHealthStatus` | EM, HDS | 健康查询 |
| `/dr/start_recording` | `dr_msgs/srv/StartRecording` | TE, Gateway | 开始录制 |
| `/dr/stop_recording` | `dr_msgs/srv/StopRecording` | TE, Gateway | 停止录制 |
| `/dr/get_recording_list` | `dr_msgs/srv/GetRecordingList` | Gateway | 列出录制 |
| `/dr/export_recording` | `dr_msgs/srv/ExportRecording` | Gateway | 导出录制 |
| `/dr/delete_recording` | `dr_msgs/srv/DeleteRecording` | Gateway | 删除录制 |
| `/dr/tag_recording` | `dr_msgs/srv/TagRecording` | TE, Gateway | 添加标签 |

---

## 5. 内部设计

### 5.1 节点架构

```
┌──────────────────────────────────────────────────────────────────────────┐
│                         DataRecorderNode                                  │
│                                                                           │
│  ┌─────────────────────────────────────────────────────────────────────┐  │
│  │                     Topic Subscriber Manager                         │  │
│  │  - 动态订阅/取消订阅目标 Topic                                        │  │
│  │  - QoS 适配（与被记录 Topic 一致）                                     │  │
│  │  - 消息序列化（mcap/rosbag2 格式）                                     │  │
│  └─────────────────────────────────────────────────────────────────────┘  │
│                                                                           │
│  ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐   │
│  │  Session Manager │    │  Storage Manager │    │  Trigger Engine  │   │
│  │  (会话管理)       │    │  (存储管理)       │    │  (触发引擎)       │   │
│  │                  │    │                  │    │                  │   │
│  │  - 会话生命周期   │    │  - 文件写入      │    │  - 手动触发      │   │
│  │  - 元数据管理    │    │  - 压缩编码      │    │  - 故障自动触发  │   │
│  │  - 并发控制      │    │  - 配额管理      │    │  - 任务自动触发  │   │
│  └────────┬─────────┘    └────────┬─────────┘    └────────┬─────────┘   │
│           │                       │                       │             │
│  ┌────────▼───────────────────────▼───────────────────────▼─────────┐   │
│  │                         VLA Data Assembler                         │   │
│  │   (图像 + 关节状态 + 语言指令 → VLA训练数据帧)                      │   │
│  └───────────────────────────────────────────────────────────────────┘   │
│                                                                           │
│  ┌──────────────────┐    ┌──────────────────┐                          │
│  │  Pre-record      │    │  Export Manager  │                          │
│  │  Buffer          │    │  (导出管理)       │                          │
│  │  (预录缓冲区)     │    │                  │                          │
│  │                  │    │  - 格式转换      │                          │
│  │  - 环形缓冲      │    │  - 云端上传      │                          │
│  │  - 触发前数据    │    │  - 进度报告      │                          │
│  └──────────────────┘    └──────────────────┘                          │
│                                                                           │
│  ┌───────────────────────────────────────────────────────────────────┐   │
│  │                        ROS2 Service/Topic Interface               │   │
│  └───────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────────┘
```

### 5.2 关键设计决策

1. **零拷贝录制**：使用 ROS2 的 loaned message 机制，避免消息序列化时的内存拷贝
2. **mcap 格式**：使用 mcap 作为默认录制格式（高性能、跨语言、支持索引）
3. **压缩策略**：图像使用 H.264 硬件编码，点云使用 LZ4 压缩，其他数据 gzip
4. **异步写入**：录制数据先写入内存缓冲，后台线程异步刷盘，避免阻塞订阅回调
5. **VLA 帧对齐**：VLA 模式下按固定 10Hz 对齐图像和关节状态，语言指令通过 Service 注入

### 5.3 关键流程

#### 5.3.1 录制启动流程

```
TE/Gateway 调用 /dr/start_recording
  → Session Manager 创建新会话
  → Storage Manager 检查存储空间
    → 空间不足 → 返回 ERR_STORAGE_FULL
  → Topic Subscriber Manager 订阅指定 Topics
    → 动态创建订阅者，QoS 与源 Topic 匹配
  → Trigger Engine 设置触发条件（如自动停止时间）
  → 状态变为 RECORDING
  → 发布 /dr/recorder_state
  → 开始接收并缓存消息
```

#### 5.3.2 故障自动触发流程

```
HDS 发布故障事件
  → Trigger Engine 收到故障信号
  → 若当前为 ARMED 状态：
      → 自动启动录制
      → 保存预录缓冲区中触发前 30s 的数据
      → 继续录制触发后 60s
      → 自动停止并保存
      → 标记 trigger_type = "auto_fault"
      → 上报 Gateway 有黑匣子数据待查看
```

#### 5.3.3 VLA 数据录制流程

```
TE 启动 VLA 任务
  → 调用 /dr/start_recording (is_vla_mode=true)
  → VLA Data Assembler 启动 10Hz 定时器
  → 每 100ms：
      → 获取最新图像帧
      → 获取最新关节状态（从 /mc/mc_state）
      → 获取最新语言指令（从 Agent）
      → 对齐时间戳
      → 封装为 VlaDataFrame
      → 发布 /dr/vla_data_frame
      → 写入 mcap 文件
  → 任务结束：
      → 停止录制
      → 生成 VLA 数据集元数据（JSON）
      → 保存到 /opt/robot/data/vla/
```

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|------|------|------|------|
| ALL | ALL → DR | 各模块 Topic（动态订阅） | 被录制数据 |
| TE | TE → DR | `/dr/start_recording` (Service) | 启动录制 |
| TE | TE → DR | `/dr/stop_recording` (Service) | 停止录制 |
| Gateway | Gateway → DR | `/dr/start_recording` (Service) | 云端触发录制 |
| Gateway | Gateway → DR | `/dr/export_recording` (Service) | 导出数据 |
| Gateway | DR → Gateway | `/dr/recorder_state` (Topic) | 录制状态同步 |
| SM | SM → DR | `/sm/transition_event` (Topic) | 状态转换事件记录 |
| HDS | HDS → DR | `/hds/health_report` (Topic) | 故障自动触发录制 |
| Agent | Agent → DR | `/dr/vla_data_frame` 注入 | VLA 语言指令 |
| Setting | DR → Setting | `/setting/get_parameter` (Service) | 读取录制配置 |
| EM | EM → DR | `/dr/get_health_status` (Service) | 健康检查 |

### 6.2 关键交互时序

#### 时序：VLA 数据录制

```
TE            DR           Agent         MC       Camera
  │             │            │            │          │
  │─start_rec──►│            │            │          │
  │  vla_mode   │            │            │          │
  │             │            │            │          │
  │             │─10Hz定时器─│            │          │
  │             │            │            │          │
  │             │─get_instruction───────►│            │
  │             │◄─instruction───────────│            │
  │             │            │            │          │
  │             │─get_joint_state───────────────────►│
  │             │◄─joint_state───────────────────────│
  │             │            │            │          │
  │             │─get_image────────────────────────────────────►│
  │             │◄─image────────────────────────────────────────│
  │             │            │            │          │
  │             │─assemble_vla_frame─────│            │          │
  │             │            │            │          │
  │             │─save_to_mcap───────────│            │          │
  │             │            │            │          │
  │             │ ...（每100ms重复）      │            │          │
  │             │            │            │          │
  │─stop_rec───►│            │            │          │
  │             │            │            │          │
```

---

## 7. 关键参数与配置

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `data_directory` | string | "/opt/robot/data/recordings/" | 录制数据根目录 |
| `max_storage_bytes` | int64 | 107374182400 | 最大存储配额（100GB） |
| `default_record_duration_sec` | float | 300.0 | 默认录制时长（秒） |
| `pre_record_buffer_sec` | float | 30.0 | 预录缓冲区时长（秒） |
| `auto_record_on_fault` | bool | true | 故障时自动录制 |
| `auto_record_duration_sec` | float | 90.0 | 自动录制时长（故障前后） |
| `compression_codec` | string | "lz4" | 压缩编码（lz4/zstd/none） |
| `image_compression` | string | "h264" | 图像压缩（h264/mjpeg/none） |
| `vla_frame_rate` | float | 10.0 | VLA 数据帧率 |
| `max_concurrent_sessions` | int | 3 | 最大并发录制会话数 |
| `retention_days` | int | 30 | 数据保留天数 |

---

## 8. 错误码定义

```
# dr_msgs/msg/ErrorCode.msg
uint16 OK                        = 0
uint16 ERR_STORAGE_FULL          = 13001   # 存储空间已满
uint16 ERR_WRITE_FAILED          = 13002   # 文件写入失败
uint16 ERR_INVALID_TOPIC         = 13003   # 非法Topic名
uint16 ERR_SESSION_NOT_FOUND     = 13004   # 会话不存在
uint16 ERR_EXPORT_FAILED         = 13005   # 导出失败
uint16 ERR_COMPRESSION_FAILED    = 13006   # 压缩失败
uint16 ERR_TOO_MANY_SESSIONS     = 13007   # 并发会话过多
uint16 ERR_INVALID_FORMAT        = 13008   # 非法导出格式
uint16 ERR_DELETE_FAILED         = 13009   # 删除失败
uint16 ERR_VLA_ASSEMBLE_FAILED   = 13010   # VLA数据组装失败
uint16 ERR_ALREADY_RECORDING     = 13011   # 已在录制中
uint16 ERR_NOT_RECORDING         = 13012   # 未在录制中
```

| 错误码 | 常量名 | 说明 | 严重级别 |
|--------|--------|------|----------|
| 0 | OK | 成功 | — |
| 13001 | ERR_STORAGE_FULL | 存储空间已满 | HIGH |
| 13002 | ERR_WRITE_FAILED | 文件写入失败 | HIGH |
| 13003 | ERR_INVALID_TOPIC | 非法Topic名 | LOW |
| 13004 | ERR_SESSION_NOT_FOUND | 会话不存在 | MEDIUM |
| 13005 | ERR_EXPORT_FAILED | 导出失败 | MEDIUM |
| 13006 | ERR_COMPRESSION_FAILED | 压缩失败 | MEDIUM |
| 13007 | ERR_TOO_MANY_SESSIONS | 并发会话过多 | MEDIUM |
| 13008 | ERR_INVALID_FORMAT | 非法导出格式 | LOW |
| 13009 | ERR_DELETE_FAILED | 删除失败 | LOW |
| 13010 | ERR_VLA_ASSEMBLE_FAILED | VLA数据组装失败 | MEDIUM |
| 13011 | ERR_ALREADY_RECORDING | 已在录制中 | LOW |
| 13012 | ERR_NOT_RECORDING | 未在录制中 | LOW |

---

## 9. 安全约束

1. **零干扰原则**：录制过程不能增加被记录 Topic 的发布延迟（使用独立线程和零拷贝）
2. **存储隔离**：录制数据目录与其他系统目录隔离，避免写满根分区
3. **数据完整性**：mcap 文件写入时计算 CRC，损坏文件自动标记并跳过
4. **隐私保护**：录制图像时自动模糊人脸（如配置启用），隐私区域不记录
5. **访问控制**：录制数据文件权限设为只读（644），防止运行时意外修改
6. **循环录制保护**：循环模式保留至少 10% 存储空间作为安全余量

---

## 10. 包结构

```
dr_msgs/                # 消息定义包
├── msg/
│   ├── RecorderState.msg
│   ├── RecordingSession.msg
│   ├── VlaDataFrame.msg
│   └── Heartbeat.msg
├── srv/
│   ├── GetHealthStatus.srv
│   ├── StartRecording.srv
│   ├── StopRecording.srv
│   ├── GetRecordingList.srv
│   ├── ExportRecording.srv
│   ├── DeleteRecording.srv
│   └── TagRecording.srv
├── CMakeLists.txt
└── package.xml

dr/                     # 节点实现包
├── include/dr/
│   ├── dr_node.hpp
│   ├── topic_subscriber_manager.hpp
│   ├── session_manager.hpp
│   ├── storage_manager.hpp
│   ├── trigger_engine.hpp
│   ├── vla_data_assembler.hpp
│   ├── pre_record_buffer.hpp
│   └── export_manager.hpp
├── src/
│   ├── dr_node.cpp
│   ├── topic_subscriber_manager.cpp
│   ├── session_manager.cpp
│   ├── storage_manager.cpp
│   ├── trigger_engine.cpp
│   ├── vla_data_assembler.cpp
│   ├── pre_record_buffer.cpp
│   ├── export_manager.cpp
│   └── main.cpp
├── test/
│   ├── test_storage_manager.cpp
│   ├── test_vla_assembler.cpp
│   ├── test_trigger_engine.cpp
│   └── test_integration.cpp
├── config/
│   └── dr_params.yaml
├── launch/
│   └── dr.launch.py
├── CMakeLists.txt
└── package.xml
```

---

## 11. 关键性能指标 (KPI)

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 录制零延迟增加 | < 1ms | 对被记录 Topic 的延迟影响 |
| 写入吞吐量 | > 500MB/s | 持续写入磁盘速度 |
| 压缩率 | > 3x | 压缩后/压缩前大小比 |
| 存储配额告警 | > 85% | 触发告警的阈值 |
| VLA 帧对齐精度 | < 20ms | 图像与关节状态时间差 |
| 预录触发响应 | < 100ms | 从触发到开始保存预录数据 |
| 并发录制会话 | >= 3 | 同时进行的录制数 |
| 数据保留完整性 | > 99.9% | 正常录制数据不损坏 |
| 故障自动录制成功率 | > 95% | 故障触发自动录制的成功率 |
| CPU 占用 | < 10% | 全速录制时 |
| 内存占用 | < 512MB | 含预录缓冲区 |
