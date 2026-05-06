# Resource Collection 模块设计

## 1. 模块概述与定位

**模块名称**：Resource Collection（资源收集）

**定位**：RC 是端侧软件系统中的**运行数据与日志聚合中心**，位于应用层。与 DR（Data Recorder）专注于高质量的传感器/运动数据录制不同，RC 负责持续性地、低开销地收集系统运行时的各类轻量数据：各模块日志、性能指标、资源使用率（CPU/内存/磁盘/网络）、心跳汇总、异常事件等。RC 是系统的"轻量级 telemetry 中心"和"运行档案库"。

**核心职责**：

1. **日志聚合**：收集各 ROS2 节点的日志输出，统一格式化、分级、存储
2. **性能监控**：持续监控 CPU、内存、GPU、磁盘、网络等资源使用率
3. **心跳聚合**：订阅各模块心跳 Topic，维护模块健康状态面板
4. **事件收集**：收集状态转换、任务起止、故障发生等系统事件
5. **数据压缩归档**：按时间窗口压缩归档，支持按时间段检索
6. **轻量上报**：定期向 Gateway 上报聚合的系统状态摘要
7. **本地查询**：提供 Service 接口供其他模块/调试工具查询历史数据

**与相邻模块的边界**：

| 边界 | RC 负责 | 对方负责 |
|------|--------|---------|
| RC ↔ ALL | 订阅日志、心跳、事件 | 各模块产生日志和心跳 |
| RC ↔ Gateway | 上报系统状态摘要 | 云端通信、远程监控 |
| RC ↔ EM | 上报进程资源使用；接收进程列表 | 进程生命周期管理 |
| RC ↔ HDS | 提供历史事件查询；接收诊断结果 | 故障诊断与定级 |
| RC ↔ Setting | 读取收集策略、保留策略配置 | 参数持久化 |
| RC ↔ DR | 协调避免重复收集（RC 轻量，DR 高质量） | 高质量数据录制 |

---

## 2. 职责边界

**RC 不做的事情**（红线）：

- **不做高质量数据录制** — 不录制原始传感器图像/点云，那是 DR 的职责
- **不做故障诊断** — 只收集原始日志和指标，不做分析判断，那是 HDS 的职责
- **不做实时告警决策** — 只上报数据，告警阈值和决策由 HDS/Gateway 处理
- **不做数据修改** — 收集的数据只读，不编辑
- **不直接连接云端** — 数据上报通过 Gateway
- **不影响系统性能** — 资源收集开销必须极低（< 1% CPU）

---

## 3. 内部架构

RC 本身不维护复杂状态机，主要维护收集任务的运行状态：

### 3.1 收集器状态

| 状态 | 说明 |
|------|------|
| `COLLECTOR_IDLE` | 收集器未启动 |
| `COLLECTOR_ACTIVE` | 正在收集 |
| `COLLECTOR_PAUSED` | 已暂停 |
| `COLLECTOR_ERROR` | 收集出错（如磁盘满） |

---

## 4. ROS2 接口定义

### 4.1 消息定义 (msg)

```
# rc_msgs/msg/SystemMetrics.msg
# 系统资源指标

builtin_interfaces/Time stamp
float32 cpu_percent           # CPU 使用率（%）
float32 memory_percent        # 内存使用率（%）
uint64 memory_used_bytes      # 已用内存（字节）
uint64 memory_total_bytes     # 总内存（字节）
float32 gpu_percent           # GPU 使用率（%）
uint64 gpu_memory_used_bytes  # GPU 显存使用
uint64 gpu_memory_total_bytes # GPU 显存总量
float32 disk_percent          # 磁盘使用率（%）
uint64 disk_used_bytes        # 已用磁盘
uint64 disk_total_bytes       # 总磁盘
float32 network_rx_mbps       # 网络接收速率（Mbps）
float32 network_tx_mbps       # 网络发送速率（Mbps）
float32 temperature_celsius   # 主板温度（℃）
```

```
# rc_msgs/msg/ModuleHealth.msg
# 模块健康状态

string module_name            # 模块名称
uint8 status                  # 状态
uint8 STATUS_ONLINE    = 0
uint8 STATUS_DEGRADED  = 1
uint8 STATUS_OFFLINE   = 2
uint8 STATUS_UNKNOWN   = 3
builtin_interfaces/Time last_heartbeat
float32 heartbeat_freq_hz     # 实际心跳频率
builtin_interfaces/Time uptime_since
```

```
# rc_msgs/msg/SystemEvent.msg
# 系统事件

builtin_interfaces/Time timestamp
string event_id               # 事件唯一ID
string category               # 类别（"state_change", "task", "fault", "user_action"）
string source_module          # 来源模块
string description            # 描述
uint8 severity                # 严重级别
uint8 SEVERITY_INFO     = 0
uint8 SEVERITY_WARNING  = 1
uint8 SEVERITY_ERROR    = 2
uint8 SEVERITY_CRITICAL = 3
string json_payload           # 附加数据（JSON）
```

```
# rc_msgs/msg/LogEntry.msg
# 聚合日志条目

builtin_interfaces/Time timestamp
string module_name
string node_name
uint8 level                   # 日志级别
uint8 LEVEL_DEBUG  = 0
uint8 LEVEL_INFO   = 1
uint8 LEVEL_WARN   = 2
uint8 LEVEL_ERROR  = 3
uint8 LEVEL_FATAL  = 4
string message
string file
uint32 line
```

```
# rc_msgs/msg/Heartbeat.msg
# RC 心跳

builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message
uint32 modules_online
uint32 total_modules
float32 collection_rate_hz
```

### 4.2 服务定义 (srv)

```
# rc_msgs/srv/GetHealthStatus.srv

# Request（空）
---
# Response
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
uint32 modules_online
```

```
# rc_msgs/srv/QueryEvents.srv
# 查询系统事件

builtin_interfaces/Time start_time
builtin_interfaces/Time end_time
string category_filter        # 空=全部
uint8 min_severity            # 最小严重级别
---
# Response
bool success
SystemEvent[] events
uint32 count
```

```
# rc_msgs/srv/QueryLogs.srv
# 查询日志

builtin_interfaces/Time start_time
builtin_interfaces/Time end_time
string module_filter          # 空=全部
uint8 min_level               # 最小日志级别
---
# Response
bool success
LogEntry[] logs
uint32 count
```

```
# rc_msgs/srv/QueryMetrics.srv
# 查询历史指标

builtin_interfaces/Time start_time
builtin_interfaces/Time end_time
string metric_name            # "cpu", "memory", "gpu", "disk", "network"
---
# Response
bool success
SystemMetrics[] metrics
uint32 count
```

```
# rc_msgs/srv/GetSystemSummary.srv
# 获取当前系统摘要

# Request（空）
---
# Response
bool success
SystemMetrics current_metrics
ModuleHealth[] module_healths
uint32 total_events_24h
uint32 total_errors_24h
float32 avg_cpu_24h
float32 avg_memory_24h
```

### 4.3 接口汇总表

#### Topics（RC 订阅）

| 名称 | 类型 | 来源 | 说明 |
|------|------|------|------|
| `/sm/robot_state` | `sm_msgs/msg/RobotState` | SM | 状态转换事件 |
| `/sm/transition_event` | `sm_msgs/msg/TransitionEvent` | SM | 转换详情 |
| `/*/heartbeat` | 各模块 Heartbeat | ALL | 各模块心跳 |
| `/te/task_state` | `te_msgs/msg/TaskState` | TE | 任务事件 |
| `/hds/health_report` | `hds_msgs/msg/HealthReport` | HDS | 健康事件 |

#### Topics（RC 发布）

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/rc/system_metrics` | `rc_msgs/msg/SystemMetrics` | RC → ALL | Reliable + Volatile + Depth 1 | 1 Hz | 系统资源指标 |
| `/rc/module_health` | `rc_msgs/msg/ModuleHealth[]` | RC → ALL | Reliable + Volatile + Depth 1 | 1 Hz | 模块健康面板 |
| `/rc/system_event` | `rc_msgs/msg/SystemEvent` | RC → ALL | Reliable + Volatile + Depth 100 | 事件驱动 | 系统事件 |
| `/rc/heartbeat` | `rc_msgs/msg/Heartbeat` | RC → EM/HDS | Reliable + Volatile + Depth 1 | 1 Hz | 心跳 |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/rc/get_health_status` | `rc_msgs/srv/GetHealthStatus` | EM, HDS | 健康查询 |
| `/rc/query_events` | `rc_msgs/srv/QueryEvents` | HDS, Gateway | 查询事件 |
| `/rc/query_logs` | `rc_msgs/srv/QueryLogs` | HDS, Gateway | 查询日志 |
| `/rc/query_metrics` | `rc_msgs/srv/QueryMetrics` | Gateway | 查询指标 |
| `/rc/get_system_summary` | `rc_msgs/srv/GetSystemSummary` | Gateway, EM | 系统摘要 |

---

## 5. 内部设计

### 5.1 节点架构

```
┌──────────────────────────────────────────────────────────────────────────┐
│                         ResourceCollectionNode                            │
│                                                                           │
│  ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐   │
│  │  Log Collector   │    │  Metrics         │    │  Event           │   │
│  │  (日志收集器)     │    │  Collector       │    │  Collector       │   │
│  │                  │    │  (指标收集器)     │    │  (事件收集器)     │   │
│  │  - rosout 订阅   │    │  - /proc 读取    │    │  - 状态转换事件   │   │
│  │  - 格式化存储    │    │  - GPU 查询      │    │  - 任务起止事件   │   │
│  │  - 级别过滤      │    │  - 网络统计      │    │  - 故障事件       │   │
│  └────────┬─────────┘    └────────┬─────────┘    └────────┬─────────┘   │
│           │                       │                       │             │
│  ┌────────▼───────────────────────▼───────────────────────▼─────────┐   │
│  │                         Data Store                                 │   │
│  │   (环形缓冲 → 时间分片 → 压缩归档 → 磁盘存储)                       │   │
│  │                                                                    │   │
│  │   - 日志：按小时分片，gzip 压缩                                    │   │
│  │   - 指标：按 10s 聚合，降采样存储                                  │   │
│  │   - 事件：结构化存储，支持索引                                     │   │
│  └────────┬───────────────────────────────────────────────────────┬───┘   │
│           │                                                       │       │
│  ┌────────▼─────────┐   ┌──────────────────┐   ┌────────────────▼───┐   │
│  │  Heartbeat       │   │  Summary         │   │  Query Engine      │   │
│  │  Monitor         │   │  Generator       │   │  (查询引擎)         │   │
│  │  (心跳监控)       │   │  (摘要生成器)     │   │                    │   │
│  │                  │   │                  │   │  - 时间范围查询      │   │
│  │  - 订阅各模块心跳 │   │  - 24h 统计聚合   │   │  - 模块过滤          │   │
│  │  - 检测超时离线   │   │  - 趋势分析       │   │  - 级别过滤          │   │
│  │  - 生成健康面板   │   │  - 上报 Gateway   │   │  - 分页返回          │   │
│  └──────────────────┘   └──────────────────┘   └────────────────────┘   │
│                                                                           │
│  ┌───────────────────────────────────────────────────────────────────┐   │
│  │                        ROS2 Service/Topic Interface               │   │
│  └───────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────────┘
```

### 5.2 关键设计决策

1. **极低开销**：资源收集使用独立线程，采样间隔 1s，单次采样 < 5ms
2. **rosout 聚合**：通过订阅 `/rosout` Topic 收集日志，不直接读取日志文件
3. **心跳超时检测**：模块心跳间隔超过 2 倍预期值 → 标记 OFFLINE
4. **时间分片存储**：数据按小时分片存储，过期自动压缩归档
5. **降采样策略**：超过 7 天的指标数据从 1Hz 降采样到 1/60Hz（每分种一个点）

### 5.3 关键流程

#### 5.3.1 系统指标收集流程

```
1Hz 定时器触发
  → Metrics Collector:
      → 读取 /proc/stat, /proc/meminfo
      → 查询 GPU 使用率（nvml 或 vendor API）
      → 读取磁盘使用情况（statfs）
      → 读取网络接口统计（/proc/net/dev）
      → 读取传感器温度（thermal_zone）
  → 封装为 SystemMetrics
  → 发布 /rc/system_metrics
  → 写入 Data Store（10s 聚合缓冲）
```

#### 5.3.2 模块健康监控流程

```
订阅各模块 /heartbeat
  → Heartbeat Monitor:
      → 维护模块最后心跳时间
      → 每 1s 检查：
          → 当前时间 - 最后心跳 > 2 × 预期间隔 → 标记 DEGRADED
          → 当前时间 - 最后心跳 > 5 × 预期间隔 → 标记 OFFLINE
      → 封装为 ModuleHealth[]
      → 发布 /rc/module_health
      → OFFLINE 模块生成 SystemEvent 上报
```

#### 5.3.3 事件收集流程

```
订阅 /sm/transition_event, /te/task_state, /hds/health_report
  → Event Collector:
      → 解析事件内容
      → 映射到统一 SystemEvent 格式
      → 写入 Data Store
      → 发布 /rc/system_event
      → CRITICAL 级别事件立即上报 Gateway
```

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|------|------|------|------|
| ALL | ALL → RC | `/rosout` (Topic) | 各模块日志 |
| ALL | ALL → RC | `/*/heartbeat` (Topic) | 各模块心跳 |
| SM | SM → RC | `/sm/transition_event` (Topic) | 状态转换事件 |
| TE | TE → RC | `/te/task_state` (Topic) | 任务事件 |
| HDS | HDS → RC | `/hds/health_report` (Topic) | 健康事件 |
| HDS | RC → HDS | `/rc/query_events` (Service) | 事件查询 |
| HDS | RC → HDS | `/rc/query_logs` (Service) | 日志查询 |
| Gateway | RC → Gateway | `/rc/system_metrics` (Topic) | 指标上报 |
| Gateway | RC → Gateway | `/rc/system_event` (Topic) | 事件上报 |
| Gateway | Gateway → RC | `/rc/get_system_summary` (Service) | 系统摘要 |
| EM | EM → RC | `/rc/get_health_status` (Service) | 健康检查 |
| EM | RC → EM | `/rc/module_health` (Topic) | 模块健康面板 |
| Setting | RC → Setting | `/setting/get_parameter` (Service) | 读取配置 |
| DR | RC ↔ DR | 协调收集范围 | 避免重复 |

### 6.2 关键交互时序

#### 时序：系统状态监控循环

```
各模块          RC           Gateway       HDS
  │             │              │            │
  │─heartbeat──►│              │            │
  │             │              │            │
  │             │─health_panel─►│            │
  │             │              │            │
  │             │─metrics─────►│            │
  │             │              │            │
  │─fault_event─►│             │            │
  │             │              │            │
  │             │─event────────►│            │
  │             │              │            │
  │             │              │            │
  │             │─query_events─────────────►│
  │             │◄─events───────────────────│
  │             │              │            │
```

---

## 7. 关键参数与配置

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `data_directory` | string | "/opt/robot/data/resources/" | 数据存储目录 |
| `metrics_collection_interval_sec` | int | 1 | 指标收集间隔（秒） |
| `log_collection_enabled` | bool | true | 是否收集日志 |
| `heartbeat_timeout_multiplier` | float | 2.0 | 心跳超时倍数 |
| `heartbeat_offline_multiplier` | float | 5.0 | 离线判定倍数 |
| `storage_retention_days` | int | 7 | 原始数据保留天数 |
| `archive_retention_days` | int | 90 | 归档数据保留天数 |
| `metrics_downsample_after_days` | int | 7 | 几天后开始降采样 |
| `max_log_entries_per_query` | int | 10000 | 单次查询最大日志数 |
| `summary_report_interval_sec` | int | 300 | 摘要上报间隔（秒） |
| `critical_event_immediate_report` | bool | true | 严重事件立即上报 |

---

## 8. 错误码定义

```
# rc_msgs/msg/ErrorCode.msg
uint16 OK                        = 0
uint16 ERR_STORAGE_FULL          = 16001   # 存储空间已满
uint16 ERR_WRITE_FAILED          = 16002   # 文件写入失败
uint16 ERR_QUERY_FAILED          = 16003   # 查询失败
uint16 ERR_INVALID_TIME_RANGE    = 16004   # 非法时间范围
uint16 ERR_COMPRESSION_FAILED    = 16005   # 压缩失败
uint16 ERR_METRICS_READ_FAILED   = 16006   # 指标读取失败
uint16 ERR_LOG_PARSE_FAILED      = 16007   # 日志解析失败
uint16 ERR_ARCHIVE_FAILED        = 16008   # 归档失败
uint16 ERR_OFFLINE_DETECTED      = 16009   # 检测到模块离线
uint16 ERR_HEARTBEAT_TIMEOUT     = 16010   # 模块心跳超时
```

| 错误码 | 常量名 | 说明 | 严重级别 |
|--------|--------|------|----------|
| 0 | OK | 成功 | — |
| 16001 | ERR_STORAGE_FULL | 存储空间已满 | HIGH |
| 16002 | ERR_WRITE_FAILED | 文件写入失败 | MEDIUM |
| 16003 | ERR_QUERY_FAILED | 查询失败 | LOW |
| 16004 | ERR_INVALID_TIME_RANGE | 非法时间范围 | LOW |
| 16005 | ERR_COMPRESSION_FAILED | 压缩失败 | LOW |
| 16006 | ERR_METRICS_READ_FAILED | 指标读取失败 | MEDIUM |
| 16007 | ERR_LOG_PARSE_FAILED | 日志解析失败 | LOW |
| 16008 | ERR_ARCHIVE_FAILED | 归档失败 | LOW |
| 16009 | ERR_OFFLINE_DETECTED | 检测到模块离线 | HIGH |
| 16010 | ERR_HEARTBEAT_TIMEOUT | 模块心跳超时 | MEDIUM |

---

## 9. 安全约束

1. **开销上限**：RC 自身 CPU 占用不得超过 1%，内存不得超过 128MB
2. **存储隔离**：数据目录与系统分区隔离，避免写满根分区
3. **日志脱敏**：收集日志时自动过滤可能包含敏感信息的字段（Token、密码等）
4. **访问控制**：历史数据文件权限只读（644），查询接口权限校验
5. **循环存储**：达到存储上限时按 LRU 删除旧数据，确保不阻塞
6. **不阻塞发布者**：rosout 订阅使用独立回调组和充足队列深度

---

## 10. 包结构

```
rc_msgs/                # 消息定义包
├── msg/
│   ├── SystemMetrics.msg
│   ├── ModuleHealth.msg
│   ├── SystemEvent.msg
│   ├── LogEntry.msg
│   └── Heartbeat.msg
├── srv/
│   ├── GetHealthStatus.srv
│   ├── QueryEvents.srv
│   ├── QueryLogs.srv
│   ├── QueryMetrics.srv
│   └── GetSystemSummary.srv
├── CMakeLists.txt
└── package.xml

rc/                     # 节点实现包
├── include/rc/
│   ├── rc_node.hpp
│   ├── log_collector.hpp
│   ├── metrics_collector.hpp
│   ├── event_collector.hpp
│   ├── heartbeat_monitor.hpp
│   ├── data_store.hpp
│   ├── summary_generator.hpp
│   └── query_engine.hpp
├── src/
│   ├── rc_node.cpp
│   ├── log_collector.cpp
│   ├── metrics_collector.cpp
│   ├── event_collector.cpp
│   ├── heartbeat_monitor.cpp
│   ├── data_store.cpp
│   ├── summary_generator.cpp
│   ├── query_engine.cpp
│   └── main.cpp
├── test/
│   ├── test_metrics_collector.cpp
│   ├── test_heartbeat_monitor.cpp
│   ├── test_query_engine.cpp
│   └── test_integration.cpp
├── config/
│   └── rc_params.yaml
├── launch/
│   └── rc.launch.py
├── CMakeLists.txt
└── package.xml
```

---

## 11. 关键性能指标 (KPI)

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 自身 CPU 占用 | < 1% | RC 进程 CPU 使用率 |
| 自身内存占用 | < 128MB | RC 进程内存使用 |
| 指标收集延迟 | < 5ms | 单次采样耗时 |
| 心跳检测延迟 | < 2s | 模块离线检测时间 |
| 日志查询响应 | < 1s | 24h 范围内日志查询 |
| 事件查询响应 | < 100ms | 事件索引查询 |
| 存储压缩率 | > 5x | 压缩后/原始大小比 |
| 数据保留完整性 | > 99% | 正常收集数据不丢失 |
| 模块健康准确率 | > 99% | 在线/离线状态判断准确 |
| 摘要上报成功率 | > 99.5% | 定期摘要上报成功率 |
| 归档任务成功率 | > 99% | 定时归档任务成功率 |
