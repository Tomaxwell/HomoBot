# FOTA 模块设计

## 1. 模块概述与定位

**模块名称**：Firmware Over The Air（FOTA）

**定位**：FOTA 是端侧软件系统中的**固件升级管理中心**，位于应用层。它负责接收云端升级指令、下载固件包、验证完整性、协调系统进入升级状态、执行升级并支持失败回滚，确保机器人软件始终处于最新且安全的状态。

**核心职责**：

1. **升级指令接收**：接收并验证来自Gateway的OTA升级命令
2. **固件下载**：通过Gateway或直连URL下载固件包，支持断点续传
3. **完整性验证**：校验固件包哈希值和数字签名
4. **状态协调**：请求SM进入UPDATING状态，升级完成后恢复
5. **升级执行**：执行固件安装（OS级A/B分区或应用级热更新）
6. **进度报告**：实时上报升级进度和状态
7. **失败回滚**：升级失败时自动回滚到上一版本
8. **版本管理**：维护已安装版本历史，支持版本查询

**与相邻模块的边界**：

| 边界 | FOTA 负责 | 对方负责 |
|------|----------|---------|
| FOTA ↔ Gateway | 接收OTA指令；请求固件下载；上报进度 | 云端通信、鉴权、下载代理 |
| FOTA ↔ SM | 请求进入UPDATING状态；通知升级完成 | 状态机决策、转换仲裁 |
| FOTA ↔ EM | 请求重启进程（应用更新后） | 进程生命周期治理 |
| FOTA ↔ HAL_EtherCAT | 触发从站固件更新（如支持） | 从站通信与烧录 |
| FOTA ↔ HDS | 上报升级过程健康状态 | 故障诊断与定级 |
| FOTA ↔ Setting | 读取更新策略配置 | 参数持久化 |

---

## 2. 职责边界

FOTA 在端侧架构中的位置：

```
┌─────────────────────────────────────────────┐
│              云端平台 / APP                   │
├─────────────────────────────────────────────┤
│              Gateway                          │
├─────────────────────────────────────────────┤
│  FOTA（本模块）  Setting   DR   RC           │  ← 应用层
├─────────────────────────────────────────────┤
│  SM    EM    HDS                            │  ← 中间件层
├─────────────────────────────────────────────┤
│  HAL_EtherCAT                               │  ← HAL层
└─────────────────────────────────────────────┘
```

**FOTA 不做的事情**（红线）：

- **不做云端通信** — 所有云端交互通过Gateway，不直接连接云端
- **不做状态机决策** — 只请求SM进入UPDATING，由SM仲裁决定
- **不做业务逻辑** — 不执行任何运动、任务、感知等业务
- **不做配置管理** — 更新策略等配置从Setting读取
- **不做数据记录** — 升级日志通过DR记录（如有需要）

---

## 3. 状态机设计

### 3.1 升级状态枚举

| 状态 | 值 | 说明 |
|------|-----|------|
| `FOTA_IDLE` | 0 | 空闲，无可用的升级任务 |
| `FOTA_DOWNLOADING` | 1 | 正在下载固件包 |
| `FOTA_VERIFYING` | 2 | 下载完成，正在验证完整性 |
| `FOTA_PREPARING` | 3 | 验证通过，正在准备升级环境 |
| `FOTA_UPDATING` | 4 | 正在执行固件安装 |
| `FOTA_REBOOTING` | 5 | 升级完成，正在重启 |
| `FOTA_COMPLETED` | 6 | 升级成功完成 |
| `FOTA_FAILED` | 7 | 升级失败（含回滚失败） |
| `FOTA_ROLLING_BACK` | 8 | 升级失败，正在回滚 |

### 3.2 状态转换图

```
                          ┌───────────────────────────────────────────┐
                          │                                           │
                    ┌─────┴──────┐  start_upgrade   ┌────────────────┴───┐
              ┌──►  │  FOTA_IDLE │─────────────────►│ FOTA_DOWNLOADING   │
              │     │     0      │                  │        1           │
              │     └────────────┘                  └────────┬───────────┘
              │           ▲                                │
              │           │         download_fail          │ success
              │           │    ┌───────────────────────────┘
              │           │    ▼
              │           │  ┌──────────────────┐   verify_success   ┌───────────────┐
              │           │  │ FOTA_VERIFYING   │──────────────────►│ FOTA_PREPARING│
              │           │  │        2         │                   │       3       │
              │           │  └────────┬─────────┘                   └───────┬───────┘
              │           │           │ verify_fail                      │ prepare_ok
              │           │    ┌──────┘                                  │
              │           │    ▼                                          ▼
              │           │  ┌──────────────────┐                ┌───────────────┐
              │           └──│   FOTA_FAILED    │◄───────────────│ FOTA_UPDATING │
              │              │        7         │   update_fail   │       4       │
              │              └────────┬─────────┘                 └───────┬───────┘
              │                       │                                 │ update_ok
              │              rollback_fail                             │
              │                       │                                 ▼
              │                       ▼                        ┌───────────────┐
              │              ┌──────────────────┐              │ FOTA_REBOOTING│
              └─────────────│ FOTA_ROLLING_BACK│              │       5       │
                            │        8         │              └───────┬───────┘
                            └────────┬─────────┘                      │ reboot_ok
                                     │                                ▼
                                     └────────────────────────────►┌───────────────┐
                                                                   │ FOTA_COMPLETED│
                                                                   │       6       │
                                                                   └───────────────┘
```

### 3.3 状态转换表

| 当前状态 | 目标状态 | 触发条件 | 优先级 | 说明 |
|----------|----------|----------|--------|------|
| IDLE | DOWNLOADING | 收到有效升级指令 | 60 | 开始下载 |
| DOWNLOADING | VERIFYING | 下载完成（100%） | — | 进入验证 |
| DOWNLOADING | FAILED | 下载失败（超时/断网/校验失败） | 60 | 尝试3次后失败 |
| VERIFYING | PREPARING | 哈希+签名验证通过 | — | 准备升级 |
| VERIFYING | FAILED | 验证不通过 | 60 | 非法固件包 |
| PREPARING | UPDATING | SM同意进入UPDATING状态 | — | 开始安装 |
| PREPARING | IDLE | SM拒绝或用户取消 | 60 | 取消升级 |
| UPDATING | REBOOTING | 安装成功 | — | 需要重启生效 |
| UPDATING | ROLLING_BACK | 安装失败或安装后校验失败 | 80 | 触发回滚 |
| REBOOTING | COMPLETED | 重启后新版本验证通过 | — | 升级完成 |
| REBOOTING | ROLLING_BACK | 重启后新版本异常 | 80 | 新系统不稳定 |
| ROLLING_BACK | IDLE | 回滚成功 | 60 | 恢复旧版本 |
| ROLLING_BACK | FAILED | 回滚也失败 | 100 | 严重故障，需人工介入 |
| * | IDLE | 用户取消（CANCEL状态） | 90 | 高优先级取消 |

### 3.4 状态转换约束

1. **下载阶段可取消**：DOWNLOADING/VERIFYING/PREPARING状态下可响应CancelUpgrade
2. **UPDATING状态不可取消**：一旦开始安装，必须等完成或失败，禁止中断
3. **ROLLING_BACK不可中断**：回滚是恢复操作，必须完整执行
4. **FAILED状态需人工确认**：进入FAILED后，必须通过Gateway人工确认才能恢复IDLE
5. **每次状态变更**必须发布 `/fota/upgrade_state` Topic

---

## 4. ROS2 接口定义

### 4.1 消息定义 (msg)

```
# fota_msgs/msg/UpgradeState.msg
# 升级状态广播

uint8 state                 # 当前升级状态
uint8 prev_state            # 上一个状态
builtin_interfaces/Time state_changed_at
string reason               # 状态变更原因
```

```
# fota_msgs/msg/UpgradeProgress.msg
# 升级进度详情

string upgrade_id           # 升级任务ID
uint8 state                 # 当前状态
float32 progress_percent    # 总体进度（0.0-100.0）
string current_phase        # 当前阶段描述
uint64 bytes_downloaded     # 已下载字节
uint64 total_bytes          # 总字节
float32 download_speed_bps  # 下载速度（bps）
builtin_interfaces/Time estimated_completion
string version_from         # 升级前版本
string version_to           # 目标版本
```

```
# fota_msgs/msg/FirmwareVersion.msg
# 固件版本信息

string component            # 组件名（"os", "app", "ethercat_fw", "mc_policy"等）
string version              # 版本号（语义化版本，如 "1.2.3"）
string build_hash           # 构建哈希
builtin_interfaces/Time installed_at
string changelog            # 变更日志摘要
```

```
# fota_msgs/msg/UpgradePackageInfo.msg
# 升级包元数据

string package_id
string version
uint64 package_size
string checksum_sha256      # SHA256校验值
string signature            # 数字签名
string[] components         # 包含的组件列表
builtin_interfaces/Time released_at
string minimum_version      # 最低兼容版本
```

```
# fota_msgs/msg/Heartbeat.msg
# FOTA 心跳

builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message
```

### 4.2 服务定义 (srv)

```
# fota_msgs/srv/GetHealthStatus.srv

# Request（空）
---
# Response
bool success
string node_name
builtin_interfaces/Time uptime_since
uint8 state
bool healthy
string message
```

```
# fota_msgs/srv/StartUpgrade.srv
# 启动升级

UpgradePackageInfo package_info
bool force                  # 是否强制升级（跳过版本检查）
---
# Response
bool accepted
uint16 error_code
string message
string upgrade_id
```

```
# fota_msgs/srv/GetUpgradeStatus.srv
# 查询当前升级状态

# Request（空）
---
# Response
bool has_active_upgrade
UpgradeProgress progress
```

```
# fota_msgs/srv/CancelUpgrade.srv
# 取消升级（仅在早期阶段有效）

string upgrade_id
---
# Response
bool cancelled
uint16 error_code
string message
```

```
# fota_msgs/srv/GetFirmwareVersion.srv
# 查询已安装版本

string component            # 空字符串表示查询全部
---
# Response
bool success
FirmwareVersion[] versions
```

### 4.3 接口汇总表

#### Topics

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/fota/upgrade_state` | `fota_msgs/msg/UpgradeState` | FOTA → ALL | Reliable + Transient Local + Depth 1 | 事件驱动 | 升级状态广播 |
| `/fota/upgrade_progress` | `fota_msgs/msg/UpgradeProgress` | FOTA → ALL | Reliable + Volatile + Depth 1 | 1-10 Hz | 升级进度 |
| `/fota/heartbeat` | `fota_msgs/msg/Heartbeat` | FOTA → EM/HDS | Reliable + Volatile + Depth 1 | 1 Hz | 心跳 |

#### Services

| 名称 | 类型 | 调用方 | 说明 |
|------|------|--------|------|
| `/fota/get_health_status` | `fota_msgs/srv/GetHealthStatus` | EM, HDS | 健康查询 |
| `/fota/start_upgrade` | `fota_msgs/srv/StartUpgrade` | Gateway | 启动升级 |
| `/fota/get_upgrade_status` | `fota_msgs/srv/GetUpgradeStatus` | Gateway, EM | 查询状态 |
| `/fota/cancel_upgrade` | `fota_msgs/srv/CancelUpgrade` | Gateway | 取消升级 |
| `/fota/get_firmware_version` | `fota_msgs/srv/GetFirmwareVersion` | Gateway, EM | 查询版本 |

---

## 5. 内部设计

### 5.1 节点架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                         FotaNode                                     │
│                                                                      │
│  ┌──────────────────┐    ┌──────────────────┐                       │
│  │  Upgrade State   │    │  Download        │                       │
│  │  Machine         │    │  Manager         │                       │
│  │  (升级状态机)     │    │  (下载管理)       │                       │
│  │                  │    │                  │                       │
│  │  - 状态跟踪      │    │  - HTTP下载      │                       │
│  │  - 转换控制      │    │  - 断点续传      │                       │
│  │  - 异常处理      │    │  - 进度回调      │                       │
│  └────────┬─────────┘    └────────┬─────────┘                       │
│           │                       │                                  │
│  ┌────────▼───────────────────────▼─────────┐                       │
│  │         Package Validator                 │                       │
│  │   (SHA256校验 → 签名验证 → 版本兼容检查)   │                       │
│  └────────┬───────────────────────┬─────────┘                       │
│           │                       │                                  │
│  ┌────────▼─────────┐   ┌─────────▼────────┐                       │
│  │  Installer       │   │  Rollback        │                       │
│  │  (安装器)         │   │  Manager         │                       │
│  │                  │   │  (回滚管理)       │                       │
│  │  - A/B分区切换   │   │                  │                       │
│  │  - 应用热更新    │   │  - 备份镜像      │                       │
│  │  - 从站烧录      │   │  - 恢复流程      │                       │
│  │  - 重启协调      │   │  - 失败上报      │                       │
│  └────────┬─────────┘   └─────────┬────────┘                       │
│           │                       │                                  │
│  ┌────────▼───────────────────────▼─────────┐                       │
│  │         Version Manager                   │                       │
│  │   (版本记录 → 历史管理 → 兼容性检查)       │                       │
│  └───────────────────────────────────────────┘                       │
│                                                                       │
│  ┌─────────────────────────────────────────┐                        │
│  │  ROS2 Service/Topic Interface           │                        │
│  └─────────────────────────────────────────┘                        │
└─────────────────────────────────────────────────────────────────────┘
```

### 5.2 关键设计决策

1. **A/B分区更新**：OS级固件使用A/B双分区，升级时写入非活动分区，验证通过后切换启动分区，失败时自动回滚
2. **应用级热更新**：ROS2节点更新时，由EM按依赖顺序停止旧版本、启动新版本，无需整机重启
3. **下载断点续传**：支持HTTP Range请求，网络中断后可从断点继续，避免重复下载
4. **安装前备份**：UPDATING前自动备份当前版本关键文件到rollback目录
5. **重启后健康检查**：REBOOTING状态后，系统启动时FOTA验证新版本健康（检查关键进程、基本功能），不健康则触发回滚

### 5.3 关键流程

#### 5.3.1 完整升级流程

```
Gateway调用 /fota/start_upgrade
  → FOTA验证package_info（版本格式、大小、校验值格式）
    → 验证失败 → 返回ERR_INVALID_PACKAGE
  → FOTA请求SM /sm/request_transition → UPDATING
    → SM拒绝 → 返回ERR_SM_TRANSITION_REJECTED
  → 状态变为 DOWNLOADING
  → 调用 /gateway/request_ota_download
    → Gateway下载固件包
    → 进度通过 /fota/upgrade_progress 上报
  → 下载完成 → 状态变为 VERIFYING
  → Package Validator:
      1. SHA256校验
      2. 数字签名验证
      3. 版本兼容性检查（minimum_version）
    → 任一失败 → 状态变为 FAILED
  → 验证通过 → 状态变为 PREPARING
  → Installer 备份当前版本
  → 状态变为 UPDATING
  → Installer 执行安装（分区切换/节点更新/从站烧录）
    → 安装失败 → 状态变为 ROLLING_BACK
    → 回滚成功 → 状态变为 IDLE
    → 回滚失败 → 状态变为 FAILED
  → 安装成功 → 状态变为 REBOOTING
  → 重启系统（OS更新）或重启节点（应用更新）
  → 启动后健康检查
    → 健康 → 状态变为 COMPLETED → 请求SM恢复之前状态
    → 不健康 → 状态变为 ROLLING_BACK
```

#### 5.3.2 失败回滚流程

```
安装失败或重启后不健康
  → 状态变为 ROLLING_BACK
  → Rollback Manager 读取备份镜像
  → OS级：切换回旧分区启动
  → 应用级：EM停止新版本节点，启动旧版本节点
  → 从站级：通过HAL_EtherCAT重新烧录旧固件
  → 启动后验证旧版本健康
    → 健康 → 状态变为 IDLE
    → 不健康 → 状态变为 FAILED（需人工介入）
  → 上报HDS升级失败事件
```

#### 5.3.3 进度报告流程

```
Download Manager / Installer 报告进度
  → Upgrade State Machine 更新内部进度
  → 封装 UpgradeProgress
  → 发布 /fota/upgrade_progress (1-10 Hz)
  → Gateway订阅后转发至云端
  → APP用户看到实时进度
```

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|------|------|------|------|
| Gateway | Gateway → FOTA | `/fota/start_upgrade` (Service) | 转发OTA启动指令 |
| Gateway | FOTA → Gateway | `/gateway/request_ota_download` (Service) | 请求下载固件 |
| Gateway | FOTA → Gateway | `/fota/upgrade_progress` (Topic) | 上报进度 |
| SM | FOTA → SM | `/sm/request_transition` (Service) | 请求UPDATING状态 |
| SM | SM → FOTA | `/sm/robot_state` (Topic) | FOTA订阅状态 |
| EM | FOTA → EM | `/em/restart_process` (Service) | 重启更新后的进程 |
| EM | EM → FOTA | `/fota/get_health_status` (Service) | 健康检查 |
| HAL_EtherCAT | FOTA → HAL_EtherCAT | `/hal_ethercat/update_slave_fw` (Service) | 从站固件更新 |
| HDS | FOTA → HDS | `/hds/report_diagnosis` (Service) | 上报升级异常 |
| Setting | FOTA → Setting | `/setting/get_parameter` (Service) | 读取更新策略 |

### 6.2 关键交互时序

#### 时序：完整OTA升级

```
云端         Gateway      FOTA        SM        Installer     EM
 │            │            │           │            │           │
 │─OTA指令───►│            │           │            │           │
 │            │─start_up──►│           │            │           │
 │            │            │           │            │           │
 │            │            │─req_transition─►│      │           │
 │            │            │             │      │           │
 │            │            │◄──accepted──│      │           │
 │            │            │   (UPDATING)│      │           │
 │            │            │           │            │           │
 │            │◄─accepted──│           │            │           │
 │◄─开始下载───│            │           │            │           │
 │            │            │           │            │           │
 │            │            │─req_dl────►│(Gateway下载)          │
 │            │            │◄──文件路径─│            │           │
 │            │            │           │            │           │
 │            │            │─验证─────►│            │           │
 │            │            │◄──通过────│            │           │
 │            │            │           │            │           │
 │            │            │─备份─────►│            │           │
 │            │            │           │            │           │
 │            │            │─安装─────►│            │           │
 │            │            │           │─执行安装──►│           │
 │            │            │           │            │           │
 │            │◄─进度──────│           │            │           │
 │◄─进度───────│            │           │            │           │
 │            │            │           │            │           │
 │            │            │           │◄──完成────│           │
 │            │            │           │            │           │
 │            │            │           │─重启节点──►│           │
 │            │            │           │            │           │
 │            │            │           │            │─停止旧版─►│
 │            │            │           │            │─启动新版─►│
 │            │            │           │            │           │
 │            │            │─健康检查──►│            │           │
 │            │            │◄──健康────│            │           │
 │            │            │           │            │           │
 │            │            │─req_transition─►│      │           │
 │            │            │             │      │           │
 │            │            │◄──恢复STANDBY│      │           │
 │            │            │           │            │           │
 │◄─升级完成───│            │           │            │           │
```

---

## 7. 关键参数与配置

| 参数名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `download_dir` | string | "/tmp/fota/download" | 下载临时目录 |
| `backup_dir` | string | "/opt/robot/fota/backup" | 备份目录 |
| `verify_timeout_sec` | int | 300 | 验证超时（秒） |
| `install_timeout_sec` | int | 600 | 安装超时（秒） |
| `max_download_retry` | int | 3 | 下载失败最大重试次数 |
| `download_chunk_size` | int | 65536 | 下载分块大小（字节） |
| `auto_rollback_on_fail` | bool | true | 失败时自动回滚 |
| `health_check_timeout_sec` | int | 120 | 重启后健康检查超时 |
| `rollback_partition` | string | "" | 回滚分区标识（OS级） |
| `slave_update_timeout_sec` | int | 300 | 从站固件更新超时 |

---

## 8. 错误码定义

```
# fota_msgs/msg/ErrorCode.msg
uint16 OK                          = 0
uint16 ERR_INVALID_PACKAGE         = 14001   # 非法升级包
uint16 ERR_DOWNLOAD_FAILED         = 14002   # 下载失败
uint16 ERR_VERIFY_FAILED           = 14003   # 校验失败
uint16 ERR_INSTALL_FAILED          = 14004   # 安装失败
uint16 ERR_INSUFFICIENT_SPACE      = 14005   # 存储空间不足
uint16 ERR_WRONG_STATE             = 14006   # 状态不允许
uint16 ERR_SM_TRANSITION_REJECTED  = 14007   # SM拒绝状态转换
uint16 ERR_ROLLBACK_FAILED         = 14008   # 回滚失败
uint16 ERR_PACKAGE_INCOMPATIBLE    = 14009   # 升级包不兼容
uint16 ERR_SIGNATURE_INVALID       = 14010   # 数字签名无效
uint16 ERR_TIMEOUT                 = 14011   # 操作超时
uint16 ERR_ALREADY_UP_TO_DATE      = 14012   # 已是最新版本
uint16 ERR_PARTIAL_UPDATE_FAILED   = 14013   # 部分更新失败
uint16 ERR_BACKUP_FAILED           = 14014   # 备份失败
uint16 ERR_NETWORK_ERROR           = 14015   # 网络错误
```

| 错误码 | 常量名 | 说明 | 严重级别 |
|--------|--------|------|----------|
| 0 | OK | 成功 | — |
| 14001 | ERR_INVALID_PACKAGE | 非法升级包 | MEDIUM |
| 14002 | ERR_DOWNLOAD_FAILED | 下载失败 | MEDIUM |
| 14003 | ERR_VERIFY_FAILED | 校验失败 | HIGH |
| 14004 | ERR_INSTALL_FAILED | 安装失败 | HIGH |
| 14005 | ERR_INSUFFICIENT_SPACE | 存储空间不足 | HIGH |
| 14006 | ERR_WRONG_STATE | 状态不允许 | MEDIUM |
| 14007 | ERR_SM_TRANSITION_REJECTED | SM拒绝状态转换 | MEDIUM |
| 14008 | ERR_ROLLBACK_FAILED | 回滚失败 | CRITICAL |
| 14009 | ERR_PACKAGE_INCOMPATIBLE | 升级包不兼容 | HIGH |
| 14010 | ERR_SIGNATURE_INVALID | 数字签名无效 | HIGH |
| 14011 | ERR_TIMEOUT | 操作超时 | MEDIUM |
| 14012 | ERR_ALREADY_UP_TO_DATE | 已是最新版本 | LOW |
| 14013 | ERR_PARTIAL_UPDATE_FAILED | 部分更新失败 | HIGH |
| 14014 | ERR_BACKUP_FAILED | 备份失败 | HIGH |
| 14015 | ERR_NETWORK_ERROR | 网络错误 | MEDIUM |

---

## 9. 安全约束

1. **强制签名验证**：所有固件包必须携带有效数字签名，未签名或签名无效一律拒绝安装
2. **降级保护**：默认禁止降级（force=true可绕过），防止回退到已知有漏洞的版本
3. **A/B分区原子切换**：OS更新采用原子分区切换，不存在半升级状态
4. **SM状态保护**：升级前必须获得SM的UPDATING状态确认，禁止绕过
5. **回滚完整性**：每次升级前必须成功备份才能开始安装，备份失败则拒绝升级
6. **下载来源限制**：只允许从白名单URL下载固件，禁止任意URL
7. **升级窗口**：可配置允许升级的时间窗口，禁止在任务执行期间自动升级

---

## 10. 包结构

```
fota_msgs/              # 消息定义包
├── msg/
│   ├── UpgradeState.msg
│   ├── UpgradeProgress.msg
│   ├── FirmwareVersion.msg
│   ├── UpgradePackageInfo.msg
│   └── Heartbeat.msg
├── srv/
│   ├── GetHealthStatus.srv
│   ├── StartUpgrade.srv
│   ├── GetUpgradeStatus.srv
│   ├── CancelUpgrade.srv
│   └── GetFirmwareVersion.srv
├── CMakeLists.txt
└── package.xml

fota/                   # 节点实现包
├── include/fota/
│   ├── fota_node.hpp
│   ├── upgrade_state_machine.hpp
│   ├── download_manager.hpp
│   ├── package_validator.hpp
│   ├── installer.hpp
│   ├── rollback_manager.hpp
│   └── version_manager.hpp
├── src/
│   ├── fota_node.cpp
│   ├── upgrade_state_machine.cpp
│   ├── download_manager.cpp
│   ├── package_validator.cpp
│   ├── installer.cpp
│   ├── rollback_manager.cpp
│   ├── version_manager.cpp
│   └── main.cpp
├── test/
│   ├── test_state_machine.cpp
│   ├── test_package_validator.cpp
│   ├── test_installer.cpp
│   └── test_integration.cpp
├── config/
│   └── fota_params.yaml
├── launch/
│   └── fota.launch.py
├── CMakeLists.txt
└── package.xml
```

---

## 11. 关键性能指标 (KPI)

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 下载速度 | > 1MB/s | 平均下载速率 |
| 断点续传恢复时间 | < 5s | 网络中断后恢复下载 |
| 校验时间 | < 30s (1GB包) | SHA256+签名验证 |
| 应用级更新停机时间 | < 10s | 节点重启切换 |
| OS级更新总时间 | < 5min | 含重启和健康检查 |
| 回滚成功率 | > 99.5% | 需要回滚时的成功率 |
| 升级失败率 | < 0.1% | 总体升级失败比例 |
| 健康检查时间 | < 60s | 重启后验证新版本 |
| 版本查询延迟 | < 10ms | 查询已安装版本 |
| 存储占用 | < 2GB | 备份+下载缓存 |
