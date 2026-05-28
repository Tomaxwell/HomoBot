# HAL Sensor 模块详细设计

## 1. 模块概述与定位

HAL Sensor 是机器人非 EtherCAT、非视觉、非 Lidar 传感器的统一硬件抽象层，负责 IMU、触觉传感器和环境传感器的数据采集与管理。

**所处层次**：HAL & Infra 层

**核心职责**：
- 管理 IMU（惯性测量单元）数据采集与发布
- 管理夹爪触觉传感器（Touch）数据采集与发布
- 管理环境传感器（温度、湿度、电池状态）
- 统一设备心跳和健康状态上报

## 2. 职责边界

| 边界 | HAL Sensor 负责 | 对方负责 |
|------|----------------|---------|
| HAL Sensor ↔ MC | 发布 IMU 数据 | 运动控制中的状态估计 |
| HAL Sensor ↔ Perception | 发布 Touch 数据 | 多模态感知融合 |
| HAL Sensor ↔ HDS | 上报设备健康状态 | 故障定级与告警 |
| HAL Sensor ↔ EM | 进程心跳、故障上报 | 进程生命周期管理 |
| HAL Sensor ↔ HAL_Audio | 独立运行，无直接交互 | - |
| HAL Sensor ↔ HAL_Camera | 独立运行，无直接交互 | - |

## 3. 状态机设计

```
[IDLE] --初始化--> [INITIALIZING] --成功--> [READY]
                                    --失败--> [FAULT]

[READY] --正常运行--> [READY]
       --故障--> [FAULT]

[FAULT] --恢复成功--> [READY]
        --恢复失败--> [FAULT]
```

| 状态 | 值 | 说明 |
|------|-----|------|
| IDLE | 0 | 初始状态 |
| INITIALIZING | 1 | 正在初始化 IMU/Touch 设备 |
| READY | 2 | 所有设备正常运行 |
| FAULT | 3 | 至少一个设备故障 |

## 4. ROS2 接口定义

### 4.1 消息定义（msg）

```
# hal_sensor_msgs/msg/SensorHubState.msg
# 传感器集线器状态

uint8 HUB_IDLE       = 0
uint8 HUB_INITIALIZING = 1
uint8 HUB_READY        = 2
uint8 HUB_FAULT        = 3

uint8 state
bool imu_healthy
bool touch_healthy
bool env_healthy
builtin_interfaces/Time stamp
```

```
# hal_sensor_msgs/msg/TouchData.msg
# 触觉传感器数据

string sensor_id             # 传感器标识（"gripper_left", "gripper_right"）
float32[] pressures          # 各触觉点压力值（N）
float32[] temperatures       # 各点温度（C）
geometry_msgs/Point[] contact_positions  # 接触点位置（本地坐标系）
bool[] contact_detected      # 是否检测到接触
uint32 contact_count         # 当前接触点数
builtin_interfaces/Time stamp
```

```
# hal_sensor_msgs/msg/EnvData.msg
# 环境传感器数据

float32 temperature_c          # 环境温度
float32 humidity_percent     # 湿度
float32 battery_voltage      # 主电池电压
float32 battery_current      # 主电池电流
float32 battery_soc          # 电池电量百分比
uint8   power_status         # 供电状态
builtin_interfaces/Time stamp
```

```
# hal_sensor_msgs/msg/Heartbeat.msg
# HAL Sensor 心跳

builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message
bool imu_healthy
bool touch_healthy
```

```
# hal_sensor_msgs/msg/ErrorCode.msg
# 错误码定义（原名 HalSensorErrorCode.msg，应改为 ErrorCode.msg）

uint16 OK                              = 0
uint16 ERR_SENSOR_IMU_NOT_FOUND        = 20041   # IMU 设备未找到
uint16 ERR_SENSOR_IMU_INIT_FAILED      = 20042   # IMU 初始化失败
uint16 ERR_SENSOR_TOUCH_NOT_FOUND      = 20043   # Touch 设备未找到
uint16 ERR_SENSOR_TOUCH_INIT_FAILED    = 20044   # Touch 初始化失败
uint16 ERR_SENSOR_IMU_DATA_STALE       = 20045   # IMU 数据过旧
uint16 ERR_SENSOR_TOUCH_DATA_STALE     = 20046   # Touch 数据过旧

uint16 error_code
string message
```

### 4.2 服务定义（srv）

```
# hal_sensor_msgs/srv/GetHealthStatus.srv

---
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
bool imu_healthy
bool touch_healthy
```

```
# hal_sensor_msgs/srv/SetImuRate.srv
# 设置 IMU 采样率

uint16 rate_hz
---
bool success
uint16 error_code
string message
```

```
# hal_sensor_msgs/srv/SetTouchSensitivity.srv
# 设置触觉灵敏度

string sensor_id
float32 sensitivity          # 0.0~1.0
---
bool success
uint16 error_code
string message
```

### 4.3 接口汇总表

#### Topics（发布）

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/sensor/imu/data` | `sensor_msgs/Imu` | Sensor → MC/Perception | Best Effort + Volatile + Depth 1 | 200Hz | IMU 数据 |
| `/sensor/imu/temp` | `sensor_msgs/Temperature` | Sensor → HDS | Best Effort + Volatile + Depth 1 | 1Hz | IMU 温度 |
| `/sensor/touch/data` | `hal_sensor_msgs/TouchData` | Sensor → Perception/MC | Best Effort + Volatile + Depth 1 | 50Hz | 触觉数据 |
| `/sensor/env/data` | `hal_sensor_msgs/EnvData` | Sensor → ALL | Reliable + Transient Local + Depth 1 | 1Hz | 环境数据 |
| `/sensor/hub_state` | `hal_sensor_msgs/SensorHubState` | Sensor → ALL | Reliable + Transient Local + Depth 1 | 1Hz | 集线器状态 |
| `/sensor/heartbeat` | `hal_sensor_msgs/Heartbeat` | Sensor → HDS/EM | Reliable + Volatile + Depth 1 | 1Hz | 心跳 |

#### Services

| 名称 | 类型 | 说明 |
|------|------|------|
| `/sensor/get_health_status` | `GetHealthStatus` | 健康状态查询 |
| `/sensor/set_imu_rate` | `SetImuRate` | 设置 IMU 采样率 |
| `/sensor/set_touch_sensitivity` | `SetTouchSensitivity` | 设置触觉灵敏度 |

## 5. 内部设计

### 5.1 节点架构

```mermaid
flowchart TB
    subgraph HalSensorNode
        ImuDriver["ImuDriver
· SPI/I2C/UART 通信接口
· 数据解析与校准
· 200Hz 发布线程"]
        TouchDriver["TouchDriver
· I2C/USB 通信接口
· 压力/温度数据解析
· 50Hz 发布线程"]
        EnvMonitor["EnvMonitor
· 电池管理芯片读取
· 温湿度传感器读取
· 1Hz 发布线程"]
    end
```

### 5.2 关键设计决策

1. **独立线程 per 传感器类型**：IMU 200Hz、Touch 50Hz、Env 1Hz，各自独立线程避免互相干扰
2. **IMU 数据优先**：IMU 是运动控制的关键输入，线程优先级设为最高
3. **Touch 坐标系**：Touch 数据在夹爪本地坐标系下发布，由 TF 提供到全局坐标系的变换
4. **电池管理**：电池状态通过 I2C/SMBus 读取 BMS 芯片数据

### 5.3 关键流程

#### 5.3.1 系统启动流程

1. 初始化 IMU 设备（SPI/I2C/UART 打开）
2. 初始化 Touch 设备（I2C/USB 打开）
3. 初始化环境传感器（I2C 打开）
4. 启动各传感器的采集线程
5. 发布设备状态 + 心跳

#### 5.3.2 IMU 数据异常处理流程

1. 检测到 IMU 数据超出合理范围（如加速度 > 20g）
2. 标记该帧为无效，发布时置 covariance 为极大值
3. 连续 10 帧异常则上报 FAULT 状态
4. 尝试重新初始化 IMU 设备

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 交互内容 | 接口类型 |
|------|----------|----------|
| MC | 订阅 `/sensor/imu/data` | Topic |
| Perception | 订阅 `/sensor/touch/data` | Topic |
| HDS | 订阅 `/sensor/heartbeat` + `/sensor/hub_state` | Topic |
| EM | 接收进程状态、重启指令 | Service |

### 6.2 关键交互时序

```
[HalSensorNode] --200Hz Imu--> [MC]
[HalSensorNode] --50Hz Touch--> [Perception]
[HalSensorNode] --1Hz Env--> [HDS/ALL]
```

## 7. 关键参数与配置

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `imu.enabled` | bool | true | 是否启用 IMU |
| `imu.interface` | string | "i2c" | 通信接口（i2c/spi/uart） |
| `imu.device_path` | string | "/dev/i2c-1" | 设备路径 |
| `imu.rate_hz` | int | 200 | IMU 采样率 |
| `imu.frame_id` | string | "imu_frame" | IMU 坐标系 |
| `touch.enabled` | bool | true | 是否启用 Touch |
| `touch.interface` | string | "i2c" | 通信接口 |
| `touch.device_path` | string | "/dev/i2c-2" | 设备路径 |
| `touch.rate_hz` | int | 50 | Touch 采样率 |
| `touch.sensor_count` | int | 2 | 触觉传感器数量 |
| `env.enabled` | bool | true | 是否启用环境传感器 |
| `env.rate_hz` | int | 1 | 环境数据采样率 |
| `heartbeat_rate_hz` | double | 1.0 | 心跳频率 |

## 8. 错误码定义

| 错误码 | 值 | 说明 | 严重程度 |
|--------|-----|------|----------|
| OK | 0 | 正常 | - |
| ERR_SENSOR_IMU_NOT_FOUND | 20041 | IMU 设备未找到 | Critical |
| ERR_SENSOR_IMU_INIT_FAILED | 20042 | IMU 初始化失败 | Critical |
| ERR_SENSOR_TOUCH_NOT_FOUND | 20043 | Touch 设备未找到 | Major |
| ERR_SENSOR_TOUCH_INIT_FAILED | 20044 | Touch 初始化失败 | Major |
| ERR_SENSOR_IMU_DATA_STALE | 20045 | IMU 数据过旧 | Minor |
| ERR_SENSOR_TOUCH_DATA_STALE | 20046 | Touch 数据过旧 | Minor |

## 9. 安全约束

1. **IMU 数据有效性检查**：发布前校验加速度/角速度是否在合理范围内
2. **数据超时保护**：IMU 超过 50ms 无新数据则标记为 stale
3. **Touch 防抖**：接触检测需连续 3 帧以上确认，避免误触发
4. **电池低电量告警**：SOC < 20% 时发布 WARNING 级别告警

## 10. 包结构

```
hal_sensor_msgs/
    msg/
        SensorHubState.msg
        TouchData.msg
        EnvData.msg
        Heartbeat.msg
        ErrorCode.msg               # 错误码（原名 HalSensorErrorCode.msg）
    srv/
        GetHealthStatus.srv
        SetImuRate.srv
        SetTouchSensitivity.srv
    CMakeLists.txt
    package.xml

hal_sensor/
    include/hal_sensor/
        hal_sensor_node.hpp
    src/
        hal_sensor_node.cpp
    config/
        hal_sensor_params.yaml
    launch/
        hal_sensor.launch.py
    CMakeLists.txt
    package.xml
```

## 11. 关键性能指标（KPI）

| 指标 | 目标值 | 说明 |
|------|--------|------|
| IMU 发布延迟 | < 5ms | 从采集到发布 |
| IMU 频率稳定性 | ±1% | 200Hz 抖动 |
| Touch 发布延迟 | < 10ms | 从采集到发布 |
| Env 发布延迟 | < 100ms | 从采集到发布 |
| 数据丢失率 | < 0.1% | 正常运行时 |
| CPU 占用 | < 5% (RK3588) | 所有传感器同时运行 |
| 内存占用 | < 100MB | 含驱动缓冲 |

---

## 12. 平台域时间同步服务

### 12.1 设计目标

机器人端侧系统中，多传感器数据（IMU 200Hz、Camera 30Hz、Lidar 10Hz、EtherCAT 1kHz）需要在统一的时间基准下融合。HAL Sensor 作为平台域的核心基础设施模块，承担**时间同步主节点**职责，通过 gPTP（IEEE 802.1AS）或 PTP（IEEE 1588-2008）向全系统分发主时钟。

**关键需求**：
- 端到端时间同步精度 < 1ms（满足运动控制 1kHz 周期的相位对齐）
- 时间主节点故障时可自动降级到备源（如 EtherCAT DC 时钟）
- 各节点可查询当前时间同步状态和健康度

### 12.2 架构设计

```mermaid
flowchart TB
    subgraph Platform["平台域"]
        HS["HAL_Sensor<br/>Time Master Node<br/>gPTP/PTP Grandmaster"]
        HE["HAL_EtherCAT<br/>Time Slave Node<br/>EtherCAT DC 时钟"]
        HC["HAL_Camera<br/>Time Slave Node<br/>硬件触发同步"]
        HL["HAL_Lidar<br/>Time Slave Node<br/>PTP 从时钟"]
    end

    subgraph Middleware["中间件层"]
        SM["SM<br/>时间同步状态订阅"]
        MC["MC<br/>控制周期时间戳对齐"]
    end

    subgraph App["应用层"]
        Per["Perception<br/>多传感器时间戳对齐"]
        DR["DR<br/>数据录制时间基准"]
    end

    HS --"gPTP Sync<br/>Multicast"--> HE
    HS --"gPTP Sync<br/>Multicast"--> HC
    HS --"gPTP Sync<br/>Multicast"--> HL
    HS --"/platform/time_sync_status"--> SM
    HS --"/platform/time_sync_status"--> MC
    HS --"/platform/time_sync_status"--> Per
    HS --"/platform/time_sync_status"--> DR

    HE --"备用主时钟"--x|故障切换| HS
```

### 12.3 时间同步接口

```
# platform_msgs/msg/TimeSyncStatus.msg
# 时间同步状态广播（平台域通用接口）

builtin_interfaces/Time stamp          # 本消息发布时间（主时钟基准）
string master_node                     # 当前主时钟节点名（如 "hal_sensor"）
uint8 sync_protocol                    # 0=gPTP, 1=PTP, 2=NTP_FALLBACK
float32 offset_from_master_ms          # 与主时钟的偏移量（ms，主节点为 0）
float32 mean_path_delay_ms             # 平均路径延迟（ms）
uint8 clock_quality                    # 时钟质量等级（0=UNKNOWN, 1=CRITICAL, 2=WARN, 3=GOOD, 4=EXCELLENT）
uint8 slave_count                      # 当前连接的从节点数量
string[] slave_node_names              # 从节点名称列表
bool sync_active                       # 同步是否活跃
string status_message                  # 状态描述
```

```
# platform_msgs/srv/RegisterTimeSlave.srv
# 从节点注册（节点启动时向时间主节点注册）

string node_name                       # 请求注册的节点名
string hardware_interface              # 硬件接口类型（"ethercat" / "camera" / "lidar" / "sensor"）
float32 required_accuracy_ms           # 该节点所需的时间精度（ms）
---
bool success
uint16 error_code
string message
float32 granted_accuracy_ms            # 主节点承诺提供的精度
builtin_interfaces/Time current_master_time  # 当前主时钟时间（用于从节点初始对齐）
```

```
# platform_msgs/srv/UnregisterTimeSlave.srv
# 从节点注销

string node_name
---
bool success
```

### 12.4 接口汇总表

| Topic / Service | 类型 | 方向 | 频率 | 说明 |
|-----------------|------|------|------|------|
| `/platform/time_sync_status` | `platform_msgs/TimeSyncStatus` | HAL_Sensor → ALL | 1Hz | 时间同步状态广播 |
| `/platform/register_time_slave` | `platform_msgs/srv/RegisterTimeSlave` | 各节点 → HAL_Sensor | 注册时 | 从节点注册 |
| `/platform/unregister_time_slave` | `platform_msgs/srv/UnregisterTimeSlave` | 各节点 → HAL_Sensor | 注销时 | 从节点注销 |

### 12.5 关键设计决策

1. **HAL_Sensor 作为主时钟源**：IMU 的高精度时钟（通常由 MEMS 器件的温补晶振提供）经过 PPS（Pulse Per Second）校准后，可作为全系统最高精度的时间基准。HAL_Sensor 中的 `TimeSyncMaster` 组件负责 gPTP/PTP 协议栈管理
2. **gPTP 优先，PTP 回退**：若网络交换机支持 gPTP（802.1AS），则使用 gPTP（精度 < 100μs）；否则回退到软件 PTP（精度 < 1ms）；若两者均不可用，回退到 NTP（精度 < 10ms，仅用于非运动控制模块）
3. **EtherCAT DC 作为备源**：当 HAL_Sensor 的 IMU 时钟失效时，系统可切换以 EtherCAT 分布式时钟（DC）为主时钟。切换由 HAL_Sensor 检测并广播 `/platform/time_sync_status` 变更
4. **硬件触发同步**：Camera 和 Lidar 支持外部触发（PPS / trigger 信号）时，HAL_Sensor 通过 GPIO 输出同步脉冲，确保采集时间戳与主时钟对齐
5. **ROS2 消息时间戳统一**：所有 ROS2 消息的 `header.stamp` 字段必须使用主时钟基准（`CLOCK_REALTIME` 经同步校准后）。HAL_Sensor 提供 `get_master_time()` 工具函数供各模块查询

### 12.6 故障处理

| 故障场景 | 检测方式 | 响应 |
|---------|---------|------|
| IMU 时钟漂移 > 1ms | 连续 3 周期 offset > 阈值 | 上报 WARNING，尝试重新校准 |
| gPTP 同步丢失 | slave_count 下降 + mean_path_delay 异常 | 广播 `clock_quality=WARN`，触发 NTP 回退 |
| 主节点完全失效 | EM 检测到 HAL_Sensor 心跳超时 | EM 启动备用主节点（EtherCAT HAL），各从节点自动重注册 |
| 从节点未按时注册 | 启动后 10s 内未收到 RegisterTimeSlave | 该节点的数据被 Perception 标记为 "未同步"，不参与融合 |

### 12.7 包结构补充

```
platform_msgs/                    # 平台域通用消息包（新增）
    msg/
        TimeSyncStatus.msg
    srv/
        RegisterTimeSlave.srv
        UnregisterTimeSlave.srv
    CMakeLists.txt
    package.xml
```

> **注意**：`platform_msgs` 是跨 HAL 层和中间件层的共享消息包，不隶属于单一模块。时间同步服务由 HAL_Sensor 实现，但接口定义放在 platform_msgs 中以供全系统使用。
