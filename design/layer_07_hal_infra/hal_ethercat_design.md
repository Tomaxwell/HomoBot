---

# EtherCAT HAL 模块设计

## 1. 模块概述与定位

**模块名称**：EtherCAT HAL

**定位**：EtherCAT HAL 是端侧软件系统中**关节驱动硬件的唯一抽象入口**，位于 HAL & Infra 层。它内部集成 SOEM（Simple Open EtherCAT Master）实现 EtherCAT 主站功能，对外通过 ROS2 接口屏蔽底层 EtherCAT 通信细节，为上层运动控制模块（MC）提供标准化的关节指令下发和关节状态反馈通道。EtherCAT HAL 是运动控制链的**最底层软件环节**，直接决定关节控制的实时性和可靠性。

**核心职责**：
1. SOEM EtherCAT 主站初始化、从站扫描与配置
2. 1kHz 周期 PDO 通信：接收关节指令 → 下发到从站 → 读取关节状态
3. 从站状态机管理（INIT → PRE-OP → SAFE-OP → OP）
4. SDO 参数配置（从站参数读写、故障码读取）
5. EtherCAT 紧急帧（EMCY）监听与 E-Stop 信号检测
6. 对外发布关节原始状态、总线状态、心跳

**与相邻模块的边界**：

| 边界 | HAL_EtherCAT 负责 | 对方负责 |
|------|-------------------|---------|
| HAL_EtherCAT ↔ MC | PDO 通信、关节指令转发、原始状态反馈 | 运动控制算法、RL 策略、关节力矩计算 |
| HAL_EtherCAT ↔ HDS | 上报总线健康状态、从站故障码 | 故障诊断与定级 |
| HAL_EtherCAT ↔ EM | 进程生命周期（被 EM 管理） | 进程启停、故障恢复 |
| HAL_EtherCAT ↔ SM | 接收 E-Stop 信号后通过 MC 上报 SM | 全局状态机决策 |

---

## 2. 状态机设计

### 2.1 EtherCAT 总线状态机

HAL_EtherCAT 管理整个 EtherCAT 总线的生命周期状态：

| 状态 | 值 | 说明 |
|------|-----|------|
| `BUS_INIT` | 0 | 主站初始化中，未开始扫描从站 |
| `BUS_SCANNING` | 1 | 正在扫描从站拓扑 |
| `BUS_CONFIGURING` | 2 | 配置从站参数、PDO 映射 |
| `BUS_PRE_OP` | 3 | 所有从站进入 PRE-OPERATIONAL |
| `BUS_SAFE_OP` | 4 | 所有从站进入 SAFE-OPERATIONAL |
| `BUS_OPERATIONAL` | 5 | 所有从站进入 OPERATIONAL，可进行 PDO 通信 |
| `BUS_ERROR` | 6 | 总线错误（从站掉线、通信超时等） |
| `BUS_RECOVERING` | 7 | 错误恢复中 |

### 2.2 状态转换图

```
                    ┌─────────────────────────────────────────┐
                    │                                         │
              ┌─────┴─────┐  scan_complete    ┌───────────────┴───┐
   startup───►│  BUS_INIT │──────────────────►│    BUS_SCANNING   │
              └───────────┘                   └─────────┬─────────┘
                    │                                   │
                    │ scan_timeout                      │ all_slaves_found
                    ▼                                   ▼
              ┌───────────┐                       ┌───────────┐
              │ BUS_ERROR │◄──────────────────────┤BUS_CONFIGURING│
              └─────┬─────┘   config_fail         └─────┬─────┘
                    │                                   │
                    │                                   │ config_ok
                    │                                   ▼
                    │                             ┌───────────┐
                    │                             │ BUS_PRE_OP│
                    │                             └─────┬─────┘
                    │                                   │
                    │                                   │ transition_ok
                    │                                   ▼
                    │                             ┌───────────┐
                    │                             │BUS_SAFE_OP│
                    │                             └─────┬─────┘
                    │                                   │
                    │                                   │ dc_synced
                    │                                   ▼
                    │                             ┌───────────┐
                    │                             │BUS_OP     │
                    │                             └─────┬─────┘
                    │                                   │
                    │         comm_error                │
                    │◄──────────────────────────────────┤
                    │                                   │
                    └───────────────────────────────────┘
                           recovery_success → BUS_CONFIGURING
```

### 2.3 状态转换约束

1. **必须所有从站到达 OP 才能接受关节指令** — 非 OPERATIONAL 状态下，`/hal_ethercat/joint_commands` 被静默丢弃
2. **从站掉线自动进入 BUS_ERROR** — 任意从站 AL 状态异常或 WKC 不匹配时触发
3. **错误恢复需 EM 介入** — BUS_ERROR 状态下不自动恢复，需 EM 决策是否重启进程
4. **E-Stop 信号直通** — 检测到 EMCY 紧急帧时，不经过状态机，直接触发硬件急停回调

---

## 3. ROS2 接口定义

### 3.1 消息定义（msg）

```
# hal_ethercat_msgs/msg/JointCommand.msg
# 关节指令（MC → HAL_EtherCAT）

# 指令类型枚举
uint8 CMD_POSITION = 0   # 位置指令
uint8 CMD_VELOCITY = 1   # 速度指令
uint8 CMD_TORQUE   = 2   # 力矩指令
uint8 CMD_IMPEDANCE = 3  # 阻抗指令

string[] joint_names           # 关节名称列表
float64[] position             # 目标位置 [rad]
float64[] velocity             # 目标速度 [rad/s]
float64[] effort               # 目标力矩 [Nm]
float64[] kp                   # 位置环增益（阻抗模式用）
float64[] kd                   # 速度环增益（阻抗模式用）
uint8[] command_modes          # 每个关节的指令类型
builtin_interfaces/Time stamp  # 时间戳（用于超时检测）
```

```
# hal_ethercat_msgs/msg/JointState.msg
# 关节原始状态（HAL_EtherCAT → MC/DR）

string[] joint_names           # 关节名称列表
float64[] position             # 实际位置 [rad]
float64[] velocity             # 实际速度 [rad/s]
float64[] effort               # 实际力矩 [Nm]
float64[] temperature          # 驱动器温度 [°C]
float64[] current              # 电机电流 [A]
uint16[] error_code            # 从站错误码
bool[] online                  # 从站是否在线
builtin_interfaces/Time stamp  # 采样时间戳
```

```
# hal_ethercat_msgs/msg/BusStatus.msg
# EtherCAT 总线状态

uint8 BUS_INIT         = 0
uint8 BUS_SCANNING     = 1
uint8 BUS_CONFIGURING  = 2
uint8 BUS_PRE_OP       = 3
uint8 BUS_SAFE_OP      = 4
uint8 BUS_OPERATIONAL  = 5
uint8 BUS_ERROR        = 6
uint8 BUS_RECOVERING   = 7

uint8 bus_state                # 总线状态
uint16 slave_count             # 从站总数
uint16 online_slave_count      # 在线从站数
uint32 cycle_count             # 累计通信周期数
uint32 error_count             # 累计错误次数
uint32 missed_cycles           # 丢周期计数
float64 cycle_time_ms          # 实际周期时间 [ms]
float64 max_cycle_jitter_us    # 最大周期抖动 [us]
builtin_interfaces/Time stamp
```

```
# hal_ethercat_msgs/msg/EmergencyFrame.msg
# EtherCAT 紧急帧（EMCY）

uint16 slave_id                # 从站 ID
uint16 error_code              # EMCY 错误码
uint8[] error_register         # 错误寄存器
uint32 timestamp_ns            # 接收时间戳
```

```
# hal_ethercat_msgs/msg/Heartbeat.msg
# EtherCAT HAL 心跳

builtin_interfaces/Time stamp
string node_name               # 节点名
uint8 state                    # 总线状态
bool healthy                   # 总线是否健康
string status_message          # 状态描述
uint16 online_slaves           # 在线从站数
bool dc_synced                 # DC 同步是否启用
```

```
# hal_ethercat_msgs/msg/ErrorCode.msg
# 错误码定义（原名 EthercatErrorCode.msg，应改为 ErrorCode.msg）

uint16 OK                              = 0
uint16 ERR_ETHERCAT_BUS_INIT_FAILED    = 2001   # 主站初始化失败
uint16 ERR_ETHERCAT_SLAVE_NOT_FOUND    = 2002   # 期望的从站未找到
uint16 ERR_ETHERCAT_SLAVE_MISMATCH     = 2003   # 从站型号/版本不匹配
uint16 ERR_ETHERCAT_PDO_CONFIG_FAILED  = 2004   # PDO 映射配置失败
uint16 ERR_ETHERCAT_STATE_TRANSITION   = 2005   # 从站状态转换失败
uint16 ERR_ETHERCAT_COMM_TIMEOUT       = 2006   # 通信超时
uint16 ERR_ETHERCAT_WKC_MISMATCH       = 2007   # Working Counter 不匹配
uint16 ERR_ETHERCAT_EMCY_RECEIVED      = 2008   # 收到紧急帧
uint16 ERR_ETHERCAT_DC_SYNC_LOST       = 2009   # DC 同步丢失
uint16 ERR_ETHERCAT_CYCLE_OVERRUN      = 2010   # 周期超时
uint16 ERR_ETHERCAT_SDO_TIMEOUT        = 2011   # SDO 读写超时
uint16 ERR_ETHERCAT_INVALID_JOINT_COUNT = 2012   # 关节数量不匹配
uint16 ERR_ETHERCAT_COMMAND_TIMEOUT    = 2013   # 指令超时（未收到新的 joint_commands）

uint16 error_code
string message
```

### 3.2 服务定义（srv）

```
# hal_ethercat_msgs/srv/GetSlaveInfo.srv
# 获取从站信息

# Request（空）
---
# Response
hal_ethercat_msgs/SlaveInfo[] slaves
bool success
string message
```

```
# hal_ethercat_msgs/msg/SlaveInfo.msg（辅助消息）
uint16 slave_id
string name
string product_code
string revision
uint16 al_status          # 应用层状态
bool online
float64 position_offset   # 位置偏置（校准用）
```

```
# hal_ethercat_msgs/srv/ResetBus.srv
# 复位总线（故障恢复用）

# Request（空）
---
# Response
bool success
string message
uint8 new_bus_state
```

```
# hal_ethercat_msgs/srv/ReadSdo.srv
# SDO 参数读取

uint16 slave_id
uint16 index
uint8 subindex
uint32 size
---
bool success
uint8[] data
string message
```

```
# hal_ethercat_msgs/srv/WriteSdo.srv
# SDO 参数写入

uint16 slave_id
uint16 index
uint8 subindex
uint8[] data
---
bool success
string message
```

```
# hal_ethercat_msgs/srv/GetHealthStatus.srv
# 健康状态查询

---
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
uint32 total_cycles
uint32 error_cycles
```

### 3.3 接口汇总表

#### Topics

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/hal_ethercat/joint_commands` | `hal_ethercat_msgs/msg/JointCommand` | MC → HAL | Best Effort, Depth 1 | 1kHz | 关节指令输入 |
| `/hal_ethercat/joint_states` | `hal_ethercat_msgs/msg/JointState` | HAL → MC/DR | Best Effort, Depth 1 | 1kHz | 关节原始状态 |
| `/hal_ethercat/bus_status` | `hal_ethercat_msgs/msg/BusStatus` | HAL → ALL | Reliable + Volatile, Depth 1 | 1Hz | 总线状态广播 |
| `/hal_ethercat/emergency_frame` | `hal_ethercat_msgs/msg/EmergencyFrame` | HAL → MC/HDS | Reliable + Volatile, Depth 50 | 事件驱动 | EMCY 紧急帧 |
| `/hal_ethercat/heartbeat` | `hal_ethercat_msgs/msg/Heartbeat` | HAL → EM/HDS | Reliable + Volatile, Depth 1 | 1Hz | 心跳 |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/hal_ethercat/get_slave_info` | `hal_ethercat_msgs/srv/GetSlaveInfo` | MC / EM | 查询从站信息 |
| `/hal_ethercat/reset_bus` | `hal_ethercat_msgs/srv/ResetBus` | EM / HDS | 总线复位 |
| `/hal_ethercat/read_sdo` | `hal_ethercat_msgs/srv/ReadSdo` | MC / Setting | SDO 参数读取 |
| `/hal_ethercat/write_sdo` | `hal_ethercat_msgs/srv/WriteSdo` | MC / Setting | SDO 参数写入 |
| `/hal_ethercat/get_health_status` | `hal_ethercat_msgs/srv/GetHealthStatus` | EM / HDS | 健康查询 |

---

## 4. 内部设计

### 4.1 节点架构

```
┌──────────────────────────────────────────────────────────────┐
│                   EthercatHalNode                            │
│                                                              │
│  ┌──────────────────┐   ┌──────────────────┐                │
│  │  SOEM Master     │   │  PDO Manager     │                │
│  │  (soem 主站)     │   │  (PDO 映射管理)  │                │
│  │                  │   │                  │                │
│  │  - ecx_init      │   │  - TxPDO 映射    │                │
│  │  - ecx_config    │   │  - RxPDO 映射    │                │
│  │  - ecx_readstate │   │  - 数据编解码    │                │
│  │  - ecx_send/recv │   │                  │                │
│  └───────┬──────────┘   └────────┬─────────┘                │
│          │                       │                           │
│  ┌───────▼───────────────────────▼─────────┐                │
│  │         Real-time Thread                │                │
│  │   SCHED_FIFO, 1kHz cycle                │                │
│  │   ┌─────────────────────────────┐       │                │
│  │   │ 1. ecx_receive_processdata  │       │                │
│  │   │ 2. Decode RxPDO → joint_states     │                │
│  │   │ 3. Encode joint_commands → TxPDO   │                │
│  │   │ 4. ecx_send_processdata     │       │                │
│  │   │ 5. Check WKC / DC sync      │       │                │
│  │   └─────────────────────────────┘       │                │
│  └───────────────────┬─────────────────────┘                │
│                      │                                       │
│  ┌───────────────────▼───────────────────┐                   │
│  │      Lock-free Ring Buffer            │                   │
│  │   (实时线程 ↔ ROS2 线程 数据交换)      │                   │
│  └───────────────────┬───────────────────┘                   │
│                      │                                       │
│  ┌───────────────────▼───────────────────┐                   │
│  │         ROS2 Callback Thread          │                   │
│  │  ┌─────────────┐ ┌─────────────────┐  │                   │
│  │  │ JointCmd    │ │ Publishers:      │  │                   │
│  │  │ Subscriber  │ │ joint_states     │  │                   │
│  │  │             │ │ bus_status       │  │                   │
│  │  │ (将接收到的 │ │ emergency_frame  │  │                   │
│  │  │  指令写入   │ │ heartbeat        │  │                   │
│  │  │  ring buf)  │ │                  │  │                   │
│  │  └─────────────┘ └─────────────────┘  │                   │
│  │  ┌─────────────────────────────────┐  │                   │
│  │  │ Service Servers:                │  │                   │
│  │  │ get_slave_info / reset_bus      │  │                   │
│  │  │ read_sdo / write_sdo            │  │                   │
│  │  │ get_health_status               │  │                   │
│  │  └─────────────────────────────────┘  │                   │
│  └───────────────────────────────────────┘                   │
│                                                              │
│  ┌─────────────────┐   ┌─────────────────┐                  │
│  │  EMCY Handler   │   │  Watchdog       │                  │
│  │  (紧急帧监听)   │   │  (指令超时检测) │                  │
│  │  → 触发 E-Stop  │   │  (500ms 无指令 → 零力矩)         │
│  └─────────────────┘   └─────────────────┘                  │
└──────────────────────────────────────────────────────────────┘
```

### 4.2 关键设计决策

1. **实时线程隔离**：PDO 通信循环在独立的 `SCHED_FIFO` 实时线程中运行，与 ROS2 回调线程完全隔离，通过无锁环形缓冲区交换数据
2. **DC 分布式时钟**：启用 EtherCAT Distributed Clock，确保所有从站同步采样，抖动 < 50us
3. **指令超时保护**：若 500ms 未收到新的 `joint_commands`，自动将所有关节指令置零（零力矩），防止控制失联导致关节失控
4. **WKC 校验**：每次 PDO 通信后校验 Working Counter，不匹配则记录错误但不中断通信（连续 10 次不匹配才进入 BUS_ERROR）

### 4.3 关键流程

#### 4.3.1 系统启动流程

```
EM 启动 HAL_EtherCAT 进程
  → 初始化 SOEM 主站（ecx_init）
  → 扫描从站拓扑（ecx_config_init）
  → 加载从站配置（PDO 映射、参数）
  → 配置分布式时钟（ecx_configdc）
  → 将所有从站切换到 PRE-OP
  → SDO 配置从站参数（电机极对数、编码器分辨率等）
  → 将所有从站切换到 SAFE-OP
  → 将所有从站切换到 OP
  → 启动实时 PDO 线程（1kHz）
  → 发布 /hal_ethercat/bus_status (BUS_OPERATIONAL)
```

#### 4.3.2 E-Stop 硬件路径

```
硬件急停按钮按下 / 从站检测到故障
  → 从站发送 EMCY 紧急帧
  → HAL_EtherCAT 的 EMCY Handler 收到紧急帧
  → 立即将 TxPDO 中所有关节指令置零（< 1ms）
  → 发布 /hal_ethercat/emergency_frame
  → MC 订阅后触发关节刹车
  → MC 异步调用 /sm/trigger_estop 通知 SM
```

> **注意**：HAL_EtherCAT 本身不调用 SM 的 Service（避免 ROS2 Service 阻塞实时线程），EMCY 处理是纯实时线程内的操作。

#### 4.3.3 指令超时保护流程

```
实时线程每次 cycle 检查 joint_commands 时间戳
  → 当前时间 - last_cmd_stamp > 500ms
  → 自动将所有关节 effort = 0, velocity = 0
  → 保持位置指令为当前实际位置（防止位置跳变）
  → 记录错误日志（非实时线程中）
  → 发布 bus_status 错误计数 +1
```

---

## 5. 与其他模块的交互

### 5.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|------|------|------|------|
| MC | MC → HAL | `/hal_ethercat/joint_commands` (Topic) | MC 下发关节力矩/位置指令 |
| MC | HAL → MC | `/hal_ethercat/joint_states` (Topic) | HAL 反馈关节实际状态 |
| MC | HAL → MC | `/hal_ethercat/emergency_frame` (Topic) | EMCY 紧急帧通知 |
| DR | HAL → DR | `/hal_ethercat/joint_states` (Topic) | DR 记录关节原始数据 |
| EM | EM → HAL | `/hal_ethercat/reset_bus` (Service) | EM 在故障时复位总线 |
| EM | HAL → EM | `/hal_ethercat/heartbeat` (Topic) | EM 监控 HAL 进程健康 |
| HDS | HAL → HDS | `/hal_ethercat/heartbeat` + `/hal_ethercat/bus_status` | HDS 诊断总线健康 |
| HDS | HAL → HDS | `/hal_ethercat/emergency_frame` (Topic) | 故障事件上报 |
| Setting | Setting → HAL | `/hal_ethercat/read_sdo` / `write_sdo` (Service) | 读写驱动器参数 |
| SM | HAL → SM | 间接：通过 MC 调用 `/sm/trigger_estop` | HAL 不直接调 SM |

### 5.2 关键交互时序

#### 正常控制循环时序

```
  MC              HAL_EtherCAT           SOEM/从站
   │                    │                    │
   │ joint_commands     │                    │
   ├───────────────────►│                    │
   │   (1kHz, BE)       │                    │
   │                    │ ecx_receive        │
   │                    │◄───────────────────┤
   │                    │ (读取 RxPDO)       │
   │                    │                    │
   │                    │ ecx_send           │
   │                    ├───────────────────►│
   │                    │ (写入 TxPDO)       │
   │                    │                    │
   │ joint_states       │                    │
   │◄───────────────────┤                    │
   │   (1kHz, BE)       │                    │
```

> **时序说明**：MC 和 HAL_EtherCAT 的 Topic 通信不是每一 cycle 都走 ROS2 层。实际实现中，MC 和 HAL_EtherCAT 可能作为**同一进程内的两个节点**（通过 `rclcpp::Node` 多节点机制），或者通过共享内存/零拷贝机制传递数据，确保 1kHz 下延迟 < 1ms。

---

## 6. 关键参数与配置

```yaml
# hal_ethercat/config/hal_ethercat_params.yaml

hal_ethercat_node:
  ros__parameters:
    # EtherCAT 网卡接口名
    ifname: "eth0"

    # 控制周期 [us]，1kHz = 1000us
    cycle_time_us: 1000

    # 期望从站数量（配置校验用）
    expected_slave_count: 20

    # 从站配置文件路径（JSON/YAML）
    slave_config_path: "/opt/striding/config/slaves.yaml"

    # DC 分布式时钟使能
    enable_dc_sync: true

    # 指令超时时间 [ms]
    command_timeout_ms: 500

    # WKC 不匹配容忍次数
    wkc_mismatch_tolerance: 10

    # 总线状态广播频率 [Hz]
    bus_status_rate_hz: 1.0

    # 心跳频率 [Hz]
    heartbeat_rate_hz: 1.0

    # 关节状态 QoS 深度
    joint_states_qos_depth: 1

    # 紧急帧 QoS 深度
    emergency_frame_qos_depth: 50

    # SDO 超时 [ms]
    sdo_timeout_ms: 1000

    # 启动超时 [s]
    boot_timeout_sec: 30.0
```

### 从站配置示例（slaves.yaml）

```yaml
slaves:
  - alias: "left_hip_yaw"
    slave_id: 1
    product_code: 0x00000001
    revision: 0x00010001
    pdo_mapping:
      txpdo:
        - {index: 0x6064, subindex: 0x00, name: "position_actual", type: "int32", scale: 0.0001}
        - {index: 0x606C, subindex: 0x00, name: "velocity_actual", type: "int32", scale: 0.001}
        - {index: 0x6077, subindex: 0x00, name: "torque_actual", type: "int16", scale: 0.1}
        - {index: 0x603F, subindex: 0x00, name: "error_code", type: "uint16"}
      rxpdo:
        - {index: 0x607A, subindex: 0x00, name: "target_position", type: "int32", scale: 0.0001}
        - {index: 0x60FF, subindex: 0x00, name: "target_velocity", type: "int32", scale: 0.001}
        - {index: 0x6071, subindex: 0x00, name: "target_torque", type: "int16", scale: 0.1}
        - {index: 0x6060, subindex: 0x00, name: "mode_of_operation", type: "int8"}
    # ... 其他 19 个从站类似配置
```

---

## 7. 错误码定义

| 错误码 | 常量名 | 说明 | 严重级别 |
|--------|--------|------|---------|
| 0 | `OK` | 成功 | — |
| 2001 | `ERR_ETHERCAT_BUS_INIT_FAILED` | SOEM 主站初始化失败（网卡打开失败等） | CRITICAL |
| 2002 | `ERR_ETHERCAT_SLAVE_NOT_FOUND` | 期望从站未找到（从站掉线或未上电） | CRITICAL |
| 2003 | `ERR_ETHERCAT_SLAVE_MISMATCH` | 从站型号/版本与配置不匹配 | HIGH |
| 2004 | `ERR_ETHERCAT_PDO_CONFIG_FAILED` | PDO 映射配置失败 | CRITICAL |
| 2005 | `ERR_ETHERCAT_STATE_TRANSITION` | 从站状态转换失败（如 SAFE_OP → OP 失败） | HIGH |
| 2006 | `ERR_ETHERCAT_COMM_TIMEOUT` | EtherCAT 通信超时 | CRITICAL |
| 2007 | `ERR_ETHERCAT_WKC_MISMATCH` | Working Counter 不匹配（数据帧丢失） | HIGH |
| 2008 | `ERR_ETHERCAT_EMCY_RECEIVED` | 收到紧急帧（从站报错） | HIGH |
| 2009 | `ERR_ETHERCAT_DC_SYNC_LOST` | DC 分布式时钟同步丢失 | HIGH |
| 2010 | `ERR_ETHERCAT_CYCLE_OVERRUN` | 实时周期超时（执行时间 > 1ms） | HIGH |
| 2011 | `ERR_ETHERCAT_SDO_TIMEOUT` | SDO 参数读写超时 | MEDIUM |
| 2012 | `ERR_ETHERCAT_INVALID_JOINT_COUNT` | 关节数量与配置不匹配 | HIGH |
| 2013 | `ERR_ETHERCAT_COMMAND_TIMEOUT` | 超过 500ms 未收到 MC 指令 | HIGH |

---

## 8. 安全约束

### 8.1 E-Stop 硬件路径

- EMCY 紧急帧检测在**实时线程内完成**，不经过 ROS2 回调
- 检测到紧急帧后，**同一周期内**将所有关节指令置零（< 1ms）
- HAL_EtherCAT **不直接调用 SM Service**，通过 Topic 通知 MC，MC 异步上报 SM

### 8.2 指令超时保护

- 500ms 无有效指令 → 自动零力矩
- 该保护在实时线程内实现，不依赖 ROS2 通信

### 8.3 启动安全检查

- 所有从站到达 OP 状态前，不接受任何关节指令（静默丢弃）
- 从站数量与配置不匹配时，停留在 BUS_ERROR，不尝试进入 OPERATIONAL

---

## 9. 包结构

```
hal_ethercat_msgs/
├── msg/
│   ├── JointCommand.msg          # 关节指令
│   ├── JointState.msg            # 关节原始状态
│   ├── BusStatus.msg             # 总线状态
│   ├── EmergencyFrame.msg        # EMCY 紧急帧
│   ├── Heartbeat.msg             # 心跳
│   ├── ErrorCode.msg             # 错误码（原名 EthercatErrorCode.msg）
│   └── SlaveInfo.msg             # 从站信息
├── srv/
│   ├── GetSlaveInfo.srv          # 查询从站信息
│   ├── ResetBus.srv              # 总线复位
│   ├── ReadSdo.srv               # SDO 读取
│   ├── WriteSdo.srv              # SDO 写入
│   └── GetHealthStatus.srv       # 健康查询
├── CMakeLists.txt
└── package.xml

hal_ethercat/
├── include/hal_ethercat/
│   ├── hal_ethercat_node.hpp     # 主节点类
│   ├── soem_master.hpp                  # SOEM 主站封装
│   ├── pdo_manager.hpp                  # PDO 映射管理
│   ├── realtime_thread.hpp              # 实时线程（SCHED_FIFO）
│   ├── ring_buffer.hpp                  # 无锁环形缓冲区
│   ├── emcy_handler.hpp                 # 紧急帧处理
│   └── watchdog.hpp                     # 指令超时检测
├── src/
│   ├── hal_ethercat_node.cpp
│   ├── soem_master.cpp
│   ├── pdo_manager.cpp
│   ├── realtime_thread.cpp
│   ├── emcy_handler.cpp
│   └── watchdog.cpp
├── test/
│   ├── test_soem_master.cpp      # SOEM 初始化测试
│   ├── test_pdo_mapping.cpp      # PDO 映射测试
│   ├── test_realtime_loop.cpp    # 实时循环测试
│   └── test_emcy_handler.cpp     # 紧急帧测试
├── config/
│   ├── hal_ethercat_params.yaml  # 节点参数
│   └── slaves.yaml                      # 从站配置（20+ DOF）
├── launch/
│   └── hal_ethercat.launch.py
├── CMakeLists.txt
└── package.xml
```

---

## 10. 关键性能指标（KPI）

| 指标 | 目标值 |
|------|--------|
| PDO 通信周期 | 1kHz（1000us） |
| 周期抖动（jitter） | < 50us |
| 启动到 OPERATIONAL 状态 | < 10s |
| EMCY 响应延迟 | < 1ms（实时线程内） |
| 指令超时保护触发 | 500ms |
| 关节状态发布延迟 | < 1ms（共享内存/零拷贝） |
| 总线状态广播延迟 | < 10ms |
| CPU 占用 | < 2%（单核） |
| 内存占用 | < 50MB |

---

## 与 MC 的接口约定（关键）

HAL_EtherCAT 和 MC 的接口是整个运动链的**核心契约**，双方必须严格遵守：

1. **Topic 名称**：`/hal_ethercat/joint_commands` / `/hal_ethercat/joint_states`
2. **频率**：双向 1kHz，Best Effort QoS
3. **字段顺序**：`joint_names` 数组的顺序必须固定且一致，双方按索引对齐
4. **时间戳**：`stamp` 字段用于超时检测，MC 必须填充当前时间
5. **控制模式**：`command_modes` 字段指示每个关节的指令类型（位置/速度/力矩/阻抗）
6. **错误处理**：`error_code != 0` 的关节，MC 应立即停止该关节的力矩输出
