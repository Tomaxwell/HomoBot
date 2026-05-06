# 人形机器人端侧软件系统架构设计

> 版本：v1.0
> 日期：2026-05-06
> 范围：端侧本体软件全部 27 个模块

---

## 1. 文档概述

### 1.1 目的

本文档是人形机器人端侧软件系统的**顶层架构设计文档**，定义系统分层结构、模块职责边界、核心通信模式、安全架构和故障处理机制。各模块的详细设计见 \`layer_XX_*/{module}_design.md\`。

### 1.2 设计原则

| 原则 | 说明 |
|------|------|
| **单一状态源** | SM 维护全局唯一状态机，任何状态变更须经 SM 仲裁 |
| **插件化运动控制** | MC 作为协调器+插件宿主，UC/LC 为形态相关插件，MC 自身形态无关 |
| **云端入口唯一** | Gateway 是端云通信唯一出口，禁止其他模块直连云端 |
| **故障隔离** | 单模块故障不得级联崩溃，EM 负责熔断与恢复 |
| **接口先行** | 先定义 ROS2 msg/srv/action，再实现逻辑 |
| **安全优先** | 所有运动指令须经 SM 状态校验，E-Stop 路径优先级最高 |

### 1.3 术语表

| 术语 | 说明 |
|------|------|
| UC | Upper Body Control，上肢控制插件（MC 加载） |
| LC | Lower Body Control，下肢控制插件（MC 加载） |
| WBC | Whole Body Control，全身控制（QP 优化） |
| RL | Reinforcement Learning，强化学习 |
| VLA | Vision-Language-Action，视觉-语言-动作模型 |
| FSM | Finite State Machine，有限状态机 |
| E-Stop | Emergency Stop，紧急停止 |
| HAL | Hardware Abstraction Layer，硬件抽象层 |

---

## 2. 系统架构总览

### 2.1 分层架构

\`\`\`
┌─────────────────────────────────────────────────────────────────────┐
│  AI 层        Agent (VLA智能体)                                      │
│              TE (任务引擎)                                           │
├─────────────────────────────────────────────────────────────────────┤
│  交互层       Interaction (多模态人机交互总控)                        │
├─────────────────────────────────────────────────────────────────────┤
│  应用层       FOTA (固件升级)                                        │
│              Setting (设置管理)                                      │
│              DR (数据采集/VLA训练数据)                                │
│              RC (资源收集/日志聚合)                                   │
├─────────────────────────────────────────────────────────────────────┤
│  感知/规划层  Perception (视觉+Lidar感知融合)                         │
│              PnC (路径规划+行走控制)                                  │
│              VSLAM (视觉建图定位)                                     │
│              Lidar-SLAM (激光雷达建图定位)                            │
│              MapManager (地图管理)                                   │
├─────────────────────────────────────────────────────────────────────┤
│  运动层       MC (运动控制协调器，插件宿主)                            │
│              ├─ UC (上肢控制插件：IK/轨迹规划/力控/夹爪)               │
│              ├─ LC (下肢控制插件：足式RL+WBC/轮式底盘+升降柱)         │
│              MP (动作播放)                                           │
│              MS (动作流整形与调度)                                    │
├─────────────────────────────────────────────────────────────────────┤
│  中间件层     SM (全局状态机，17状态)                                 │
│              EM (进程生命周期治理)                                    │
│              Gateway (端侧网关，云端唯一出口)                          │
│              HDS (健康诊断系统)                                       │
├─────────────────────────────────────────────────────────────────────┤
│  HAL & Infra  HAL_EtherCAT (SOEM主站，EtherCAT通信)                  │
│              HAL_Camera (RealSense D435×1 + D405×2)                  │
│              HAL_Lidar (Livox Mid-360s)                              │
│              HAL_Sensor (IMU/Touch/环境传感器)                        │
│              HAL_Audio (音频+ASR+TTS)                                │
│              TF (坐标变换发布)                                        │
│              systemd (进程管理)                                       │
└─────────────────────────────────────────────────────────────────────┘
\`\`\`

### 2.2 模块全景

| 缩写 | 全称 | 层次 | 核心职责 | 设计文档 |
|------|------|------|----------|----------|
| **SM** | State Manager | 中间件层 | 全局状态机（17状态），安全闸门，状态转换仲裁 | [sm_design.md](layer_06_middleware/sm_design.md) |
| **EM** | Executive Manager | 中间件层 | 进程生命周期治理，启动编排，故障恢复，熔断 | [em_design_v2.md](layer_06_middleware/em_design_v2.md) |
| **Gateway** | Gateway | 中间件层 | 云端/APP通信唯一出口，命令转发，遥测上报 | [gateway_design.md](layer_06_middleware/gateway_design.md) |
| **TE** | Task Engine | AI层/中间件层 | 任务调度与执行，VLA任务类型，任务生命周期 | [te_design.md](layer_01_ai/te_design.md) |
| **HDS** | Health Diagnosis System | 中间件层 | 多维健康诊断，故障定级，诊断链 | [hds_design.md](layer_06_middleware/hds_design.md) |
| **Agent** | Agent | AI层 | VLA具身智能体，LLM Agent + Skills + 记忆 | [agent_design.md](layer_01_ai/agent_design.md) |
| **MC** | Motion Control | 运动层 | 运动控制协调器，插件宿主，指令仲裁与聚合，EtherCAT统一出口 | [mc_design.md](layer_05_motion/mc_design.md) |
| **UC** | Upper Body Control | 运动层 | 上肢控制插件（MC加载）：IK/轨迹规划/末端力控/夹爪/避自碰 | [uc_design.md](layer_05_motion/uc_design.md) |
| **LC** | Lower Body Control | 运动层 | 下肢控制插件（MC加载）：足式RL+WBC+步态；轮式底盘+升降柱 | [lc_design.md](layer_05_motion/lc_design.md) |
| **MP** | Motion Player | 运动层 | 预录动作序列执行，动作插值，时间同步 | [mp_design.md](layer_05_motion/mp_design.md) |
| **MS** | Motion Streamer | 运动层 | 运动指令流式整形，平滑过渡，安全限幅 | [ms_design.md](layer_05_motion/ms_design.md) |
| **PnC** | Planning and Control | 感知/规划层 | 全局路径规划，局部轨迹跟踪，动态避障，行走控制信号 | [pnc_design.md](layer_04_perception_planning/pnc_design.md) |
| **Perception** | Perception | 感知/规划层 | 视觉+Lidar多传感器融合，障碍物检测，语义分割 | [perception_design.md](layer_04_perception_planning/perception_design.md) |
| **VSLAM** | Vision SLAM | 感知/规划层 | 视觉特征提取，视觉里程计，回环检测 | [vslam_design.md](layer_04_perception_planning/vslam_design.md) |
| **Lidar-SLAM** | Lidar SLAM | 感知/规划层 | 扫描匹配，占据栅格，激光雷达定位 | [lidar_slam_design.md](layer_04_perception_planning/lidar_slam_design.md) |
| **MapManager** | Map Manager | 感知/规划层 | 地图存储/加载/切换，云端同步，生命周期管理 | [mapmanager_design.md](layer_04_perception_planning/mapmanager_design.md) |
| **DR** | Data Recorder | 应用层 | VLA训练数据采集，故障黑匣子，录制管理 | [dr_design.md](layer_03_application/dr_design.md) |
| **FOTA** | Firmware Over The Air | 应用层 | 固件下载/验证/安装/回滚 | [fota_design.md](layer_03_application/fota_design.md) |
| **Setting** | Setting | 应用层 | 统一参数存储，Schema验证，热更新 | [setting_design.md](layer_03_application/setting_design.md) |
| **RC** | Resource Collection | 应用层 | 日志聚合，性能监控，事件收集 | [rc_design.md](layer_03_application/rc_design.md) |
| **HAL_EtherCAT** | EtherCAT HAL | HAL & Infra | SOEM主站，EtherCAT从站管理，SDO/PDO通信 | [hal_ethercat_design.md](layer_07_hal_infra/hal_ethercat_design.md) |
| **HAL_Camera** | Camera HAL | HAL & Infra | RealSense相机管理（D435×1 + D405×2） | [hal_camera_design.md](layer_07_hal_infra/hal_camera_design.md) |
| **HAL_Lidar** | Lidar HAL | HAL & Infra | Livox Mid-360s固态激光雷达管理 | [hal_lidar_design.md](layer_07_hal_infra/hal_lidar_design.md) |
| **HAL_Sensor** | Sensor HAL | HAL & Infra | IMU、Touch、环境传感器管理 | [hal_sensor_design.md](layer_07_hal_infra/hal_sensor_design.md) |
| **HAL_Audio** | Audio HAL | HAL & Infra | 音频硬件抽象，ASR，TTS | [hal_audio_design.md](layer_07_hal_infra/hal_audio_design.md) |
| **TF** | TF Publisher | HAL & Infra | URDF解析，/tf + /tf_static发布 | [tf_design.md](layer_07_hal_infra/tf_design.md) |
| **Interaction** | Interaction | 交互层 | 语音/视觉/触觉/消息意图整合 | [interaction_design.md](layer_02_interaction/interaction_design.md) |

---

## 3. 核心设计决策

### 3.1 运动控制插件化架构

MC 采用**协调器+插件宿主**架构，核心决策：

- **MC 形态无关**：不实现具体控制算法，通过 C++ 纯虚接口加载 UC/LC 插件
- **UC 公共组件**：两种形态（足式/轮式）共用同一 UC 插件（上肢控制）
- **LC 形态专属**：\`lc_bipedal.so\`（足式：RL+WBC+步态）vs \`lc_wheeled.so\`（轮式：底盘+升降柱）
- **零拷贝共享内存**：MC 与 UC/LC 插件通过共享内存交换数据（1kHz 实时线程）
- **指令仲裁**：MP/MS 可通过 MotionTarget Topic 直接下发关节目标，覆盖插件输出

\`\`\`
┌─────────────────────────────────────────────┐
│  MC (协调器)                                │
│  ┌─────────┐    ┌─────────────────────┐    │
│  │ UC 插件 │    │ LC 插件             │    │
│  │(上肢)   │    │(足式RL/WBC 或 轮式) │    │
│  └────┬────┘    └──────────┬──────────┘    │
│       │                    │               │
│  ┌────▼────────────────────▼──────────┐    │
│  │  指令聚合 + 关节限位 + 安全校验     │    │
│  └──────────────┬─────────────────────┘    │
│                 │                          │
│  ┌──────────────▼─────────────────────┐    │
│  │  HAL_EtherCAT (统一下发)            │    │
│  └────────────────────────────────────┘    │
└─────────────────────────────────────────────┘
\`\`\`

### 3.2 单一状态源与运动安全闸门

SM 维护全局唯一状态机（17 状态），所有运动指令必须经过 SM 状态校验：

- \`FAULT\` / \`ACTIVE_E_STOP\` 状态下拒绝所有运动指令
- E-Stop 触发后，SM 进入 \`ACTIVE_E_STOP\`，MC 同步切断 UC/LC 插件
- 运动模式切换由 SM 触发，MC 协调 UC/LC 执行具体切换

### 3.3 云端通信入口唯一

Gateway 是端云通信的**唯一出口**：
- 所有云端命令经 Gateway 转发到目标模块
- 所有遥测数据经 Gateway 上报云端
- 禁止任何其他模块直接调用云端 API

---

## 4. 通信架构

### 4.1 通信协议矩阵

| 协议 | 用途 | 典型场景 | 频率 |
|------|------|----------|------|
| **ROS2 Topic** | 高频数据流、状态广播 | \`/joint_states\`, \`/sm/robot_state\` | 1Hz~1kHz |
| **ROS2 Service** | 同步请求/响应 | 状态转换、参数查询、健康检查 | 按需 |
| **ROS2 Action** | 长耗时操作+进度反馈 | 导航、动作播放、复合任务 | 秒~分钟级 |
| **共享内存** | 实时控制数据（零拷贝） | MC↔UC/LC 插件接口 | 1kHz |
| **MQTT** | 跨语言/跨进程通信 | EM↔HDS 进程间通信 | 按需 |

### 4.2 核心 Topic 总览

| Topic | 类型 | 发布者 | 订阅者 | 频率 | 说明 |
|-------|------|--------|--------|------|------|
| \`/sm/robot_state\` | \`sm_msgs/RobotState\` | SM | ALL | 10Hz | 全局状态广播 |
| \`/sm/transition_event\` | \`sm_msgs/TransitionEvent\` | SM | ALL | 事件 | 状态转换事件 |
| \`/mc/joint_command\` | \`sensor_msgs/JointState\` | MC | HAL_EtherCAT | 1kHz | 聚合关节指令 |
| \`/mc/whole_body_state\` | \`mc_msgs/WholeBodyState\` | MC | PnC, TE, DR | 100Hz | 全身状态 |
| \`/lc/lower_body_status\` | \`mc_msgs/LowerBodyStatus\` | MC | PnC, UC, DR | 1kHz | 下肢状态反馈 |
| \`/lc/gait_phase\` | \`mc_msgs/GaitPhase\` | MC | UC, PnC | 1kHz | 步态相位广播 |
| \`/tf\` | \`tf2_msgs/TFMessage\` | TF | ALL | 100Hz | 坐标变换 |
| \`/joint_states\` | \`sensor_msgs/JointState\` | HAL_EtherCAT | MC, UC, DR | 1kHz | 关节原始状态 |
| \`/perception/result\` | \`perception_msgs/PerceptionResult\` | Perception | PnC, Agent, TE | 30Hz | 感知融合结果 |
| \`/hds/alarm\` | \`hds_msgs/AlarmEvent\` | HDS | EM, Gateway | 事件 | 故障告警 |

### 4.3 核心 Service 总览

| Service | 类型 | 提供者 | 说明 |
|---------|------|--------|------|
| \`/sm/request_transition\` | \`sm_msgs/RequestTransition\` | SM | 状态转换请求 |
| \`/sm/trigger_estop\` | \`sm_msgs/TriggerEStop\` | SM | 急停触发（优先级=100） |
| \`/sm/is_motion_allowed\` | \`sm_msgs/IsMotionAllowed\` | SM | 运动许可查询 |
| \`/mc/set_motion_mode\` | \`mc_msgs/SetMotionMode\` | MC | 设置运动模式 |
| \`/te/submit_task\` | \`te_msgs/SubmitTask\` | TE | 提交新任务 |
| \`/gateway/forward_command\` | \`gateway_msgs/ForwardCommand\` | Gateway | 云端命令转发 |

### 4.4 核心 Action 总览

| Action | 类型 | 提供者 | 说明 |
|--------|------|--------|------|
| \`/te/execute_task\` | \`te_msgs/ExecuteTask\` | TE | 任务执行（含进度反馈） |
| \`/pnc/navigate_to\` | \`pnc_msgs/NavigateTo\` | PnC | 导航到目标点 |
| \`/mp/play_motion\` | \`mp_msgs/PlayMotion\` | MP | 动作播放 |
| \`/mc/execute_motion\` | \`mc_msgs/ExecuteMotion\` | MC | 运动执行（TE调用） |

---

## 5. 数据流架构

### 5.1 典型任务数据流（以导航+抓取任务为例）

\`\`\`
Agent/TE          PnC          Perception        MC        UC/LC      HAL_EtherCAT
  │               │               │              │           │            │
  │──SubmitTask──▶│               │              │           │            │
  │               │──订阅感知────▶│              │           │            │
  │               │◀──感知结果────│              │           │            │
  │               │──规划路径─────│─────────────▶│           │            │
  │               │               │              │──LocomotionCommand──▶│
  │               │               │              │◀──LowerBodyStatus───│
  │               │               │              │──JointCommand───────▶│
  │               │               │              │           │            │
  │               │◀──────────────│──────────────│──定位反馈────────────│
  │               │               │              │           │            │
  │◀──任务完成────│               │              │           │            │
\`\`\`

### 5.2 实时控制数据流（1kHz）

\`\`\`
HAL_EtherCAT ──JointState──▶ MC ──BaseState──▶ UC/LC 插件
                                      │
                              ┌───────┴───────┐
                              ▼               ▼
                            UC插件          LC插件
                         (IK/轨迹规划)   (RL+WBC/底盘)
                              │               │
                              └───────┬───────┘
                                      ▼
                               MC 指令聚合
                                      │
                                      ▼
                            HAL_EtherCAT 下发
\`\`\`

---

## 6. 安全架构

### 6.1 E-Stop 路径

E-Stop 是最高优先级操作，路径必须**最短、最可靠**：

\`\`\`
用户/App/遥操 ──▶ Gateway/SM ──▶ SM 状态机 ──▶ ACTIVE_E_STOP
                                              │
                                              ▼
                    MC 独立 CallbackGroup ──▶ 切断 UC/LC 插件
                                              │
                                              ▼
                                    HAL_EtherCAT ──▶ 电机刹车/零力矩
\`\`\`

**关键约束**：
- E-Stop 触发后，SM 进入 \`ACTIVE_E_STOP\`，广播状态变更
- MC 必须在独立 CallbackGroup 中监听 E-Stop，避免被其他回调阻塞
- 物理急停按钮直接切断 EtherCAT 电源（硬件级）

### 6.2 运动指令安全校验链

\`\`\`
运动指令来源 (PnC/MS/MP/TE)
        │
        ▼
┌───────────────┐
│ SM 状态校验    │──FAULT/ACTIVE_E_STOP?──▶ 拒绝
│ /sm/is_motion_allowed │
└───────────────┘
        │
        ▼
┌───────────────┐
│ MC 关节限位    │──超限?──▶ 截断至安全范围
│ 速度/力矩限幅  │
└───────────────┘
        │
        ▼
┌───────────────┐
│ UC/LC 插件     │──插件级安全校验
│ 自碰撞/接触检测 │
└───────────────┘
        │
        ▼
   HAL_EtherCAT 下发
\`\`\`

### 6.3 权限边界

| 权限 | 持有者 | 禁止行为 |
|------|--------|----------|
| 状态变更 | SM | 禁止非 SM 模块擅自变更全局状态 |
| 进程启停 | EM | 禁止非 EM 模块直接启动/停止进程 |
| 云端通信 | Gateway | 禁止非 Gateway 模块直连云端 API |
| 故障定级 | HDS | 禁止其他模块做故障定级决策 |
| 运动模式切换 | MC | 禁止 UC/LC 插件擅自切换运动模式 |

---

## 7. 故障处理架构

### 7.1 健康诊断系统（HDS）

HDS 是故障处理的**中枢决策节点**：
- 接收所有模块的原始健康数据（心跳、错误码、性能指标）
- 执行多维诊断链，输出故障等级（INFO/WARNING/ERROR/CRITICAL）
- 向 SM 建议状态降级（DEGRADED）或故障转换（FAULT）
- 通过 MQTT 向 EM 发送进程重启/熔断指令

### 7.2 故障定级与响应

| 级别 | 触发条件 | 响应 |
|------|----------|------|
| **INFO** | 轻微异常，可自愈 | 记录日志，无需动作 |
| **WARNING** | 性能下降，需关注 | 上报 HDS，可能调整参数 |
| **ERROR** | 模块功能受损 | HDS 建议 SM 降级至 DEGRADED |
| **CRITICAL** | 安全威胁或系统崩溃 | HDS 请求 SM 进入 FAULT，EM 熔断进程 |

### 7.3 EM 熔断与恢复

\`\`\`
模块故障 ──▶ HDS 诊断 ──▶ 定级 ERROR/CRITICAL
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
           SM 降级         EM 熔断         Gateway 上报
           DEGRADED       进程重启         云端告警
\`\`\`

---

## 8. 接口标准

全部 27 个模块的 ROS2 接口遵循统一标准，详见 [interface_standards.md](interface_standards.md)。

**核心规范摘要**：
- **Heartbeat.msg**：\`stamp\`, \`node_name\`, \`state\`, \`healthy\`, \`status_message\` + 扩展字段
- **GetHealthStatus.srv**：\`success\`, \`node_name\`, \`uptime_since\`, \`state\`, \`healthy\`, \`message\` + 扩展字段
- **ErrorCode.msg**：统一命名（无模块前缀），格式 \`ERR_{MODULE}_{DESCRIPTION}\`
- **Service 命名**：VerbNoun 格式（如 \`GetHealthStatus\`, \`SubmitTask\`, \`RemoveMap\`）
- **Time 字段**：统一使用 \`stamp\`
- **力矩字段**：统一使用 \`efforts\`

---

## 9. 状态机总览

### 9.1 SM 全局状态机（17 状态）

\`\`\`
                    BOOTING
                      │
                      ▼ key_modules_ready
                   STANDBY ◄────────────────────┐
                      │                          │
        ┌─────────────┼─────────────┐           │
        ▼             ▼             ▼           │
    CHARGING      UPDATING        DEBUG         │
        │             │             │           │
        └─────────────┴─────────────┘           │
                      │                          │
        ┌─────────────┼─────────────┐           │
        ▼             ▼             ▼           │
   ACTIVE_STAND   ACTIVE_READY   ACTIVE_SQUAT   │
        │             │             │           │
        └─────────────┴─────────────┘           │
                      │                          │
        ┌─────────────┼─────────────┐           │
        ▼             ▼             ▼           │
   ACTIVE_SIT   ACTIVE_MOTION   ACTIVE_WALKING  │
        │             │             │           │
        └─────────────┴─────────────┘           │
                      │                          │
        ┌─────────────┼─────────────┐           │
        ▼             ▼             ▼           │
ACTIVE_OPERATING  ACTIVE_ZERO_TORQUE  ACTIVE_DAMPING
                      │
        ┌─────────────┴─────────────┐
        ▼                           ▼
   ACTIVE_E_STOP ◄───────────────── FAULT ◄────┘
        │                           │
        └────────────► DEGRADED ◄───┘
\`\`\`

### 9.2 MC 运动模式状态机（10 模式）

| 模式 | 说明 | 足式行为 | 轮式行为 |
|------|------|----------|----------|
| \`IDLE\` | 空闲，关节使能 | 保持位置 | 保持位置 |
| \`STAND\` | 稳定站立 | 双足站立平衡 | 底盘锁定，升降柱中位 |
| \`READY\` | 预备，预紧 | 关节预紧，低刚度 | 关节预紧 |
| \`SQUAT\` | 下蹲 | 下蹲过程 | **不支持** |
| \`SIT\` | 落座 | 坐下/蹲下 | **不支持** |
| \`MOTION\` | 动作执行 | 执行动作 | 执行动作 |
| \`WALKING\` | 持续移动 | 连续步态行走 | 底盘持续移动 |
| \`OPERATING\` | 上肢操作 | 站立中手臂操作 | 底盘锁定，升降柱可调 |
| \`ZERO_TORQUE\` | 零力矩 | effort = 0 | effort = 0 |
| \`DAMPING\` | 阻尼模式 | 高阻尼 | 高阻尼 |

---

## 10. 形态适配

系统支持两种形态，通过 \`robot_type\` 参数配置：

| 形态 | \`robot_type\` | LC 插件 | UC 插件 | 适用场景 |
|------|-------------|---------|---------|----------|
| **足式人形** | \`bipedal\` | \`lc_bipedal.so\` | \`uc_common.so\` | 复杂地形行走，工业巡检，应急救援 |
| **轮式人形** | \`wheeled\` | \`lc_wheeled.so\` | \`uc_common.so\` | 商超零售，导览接待，实验室自动化 |

**形态无关设计**：
- MC 代码完全形态无关，通过插件接口与 LC 交互
- UC 插件两种形态共用
- PnC 输出统一的 \`LocomotionCommand\`（vx, vy, yaw_rate），由 LC 插件解释执行

---

## 11. 设计文档索引

### 按层次索引

| 层次 | 模块 | 设计文档 |
|------|------|----------|
| AI 层 | Agent | [layer_01_ai/agent_design.md](layer_01_ai/agent_design.md) |
| AI 层 | TE | [layer_01_ai/te_design.md](layer_01_ai/te_design.md) |
| 交互层 | Interaction | [layer_02_interaction/interaction_design.md](layer_02_interaction/interaction_design.md) |
| 应用层 | DR | [layer_03_application/dr_design.md](layer_03_application/dr_design.md) |
| 应用层 | FOTA | [layer_03_application/fota_design.md](layer_03_application/fota_design.md) |
| 应用层 | RC | [layer_03_application/rc_design.md](layer_03_application/rc_design.md) |
| 应用层 | Setting | [layer_03_application/setting_design.md](layer_03_application/setting_design.md) |
| 感知/规划层 | Perception | [layer_04_perception_planning/perception_design.md](layer_04_perception_planning/perception_design.md) |
| 感知/规划层 | PnC | [layer_04_perception_planning/pnc_design.md](layer_04_perception_planning/pnc_design.md) |
| 感知/规划层 | VSLAM | [layer_04_perception_planning/vslam_design.md](layer_04_perception_planning/vslam_design.md) |
| 感知/规划层 | Lidar-SLAM | [layer_04_perception_planning/lidar_slam_design.md](layer_04_perception_planning/lidar_slam_design.md) |
| 感知/规划层 | MapManager | [layer_04_perception_planning/mapmanager_design.md](layer_04_perception_planning/mapmanager_design.md) |
| 运动层 | MC | [layer_05_motion/mc_design.md](layer_05_motion/mc_design.md) |
| 运动层 | UC | [layer_05_motion/uc_design.md](layer_05_motion/uc_design.md) |
| 运动层 | LC | [layer_05_motion/lc_design.md](layer_05_motion/lc_design.md) |
| 运动层 | MP | [layer_05_motion/mp_design.md](layer_05_motion/mp_design.md) |
| 运动层 | MS | [layer_05_motion/ms_design.md](layer_05_motion/ms_design.md) |
| 中间件层 | SM | [layer_06_middleware/sm_design.md](layer_06_middleware/sm_design.md) |
| 中间件层 | EM | [layer_06_middleware/em_design_v2.md](layer_06_middleware/em_design_v2.md) |
| 中间件层 | Gateway | [layer_06_middleware/gateway_design.md](layer_06_middleware/gateway_design.md) |
| 中间件层 | HDS | [layer_06_middleware/hds_design.md](layer_06_middleware/hds_design.md) |
| HAL & Infra | HAL_EtherCAT | [layer_07_hal_infra/hal_ethercat_design.md](layer_07_hal_infra/hal_ethercat_design.md) |
| HAL & Infra | HAL_Camera | [layer_07_hal_infra/hal_camera_design.md](layer_07_hal_infra/hal_camera_design.md) |
| HAL & Infra | HAL_Lidar | [layer_07_hal_infra/hal_lidar_design.md](layer_07_hal_infra/hal_lidar_design.md) |
| HAL & Infra | HAL_Sensor | [layer_07_hal_infra/hal_sensor_design.md](layer_07_hal_infra/hal_sensor_design.md) |
| HAL & Infra | HAL_Audio | [layer_07_hal_infra/hal_audio_design.md](layer_07_hal_infra/hal_audio_design.md) |
| HAL & Infra | TF | [layer_07_hal_infra/tf_design.md](layer_07_hal_infra/tf_design.md) |

### 跨模块规范

| 文档 | 说明 |
|------|------|
| [interface_standards.md](interface_standards.md) | ROS2 接口标准规范（Heartbeat/GetHealthStatus/ErrorCode/命名约定） |

---

## 12. 用例索引

| 编号 | 用例 | 核心模块 | 文档 |
|------|------|----------|------|
| UC-01 | 工业设施巡检 | PnC, Perception, VSLAM, LC(bipedal) | [use_cases/use_case_01_industrial_inspection.md](use_cases/use_case_01_industrial_inspection.md) |
| UC-02 | 工厂柔性装配 | UC, Perception, MP, MS | [use_cases/use_case_02_factory_assembly.md](use_cases/use_case_02_factory_assembly.md) |
| UC-03 | 商超零售拣货 | UC, LC(wheeled), Perception, Agent | [use_cases/use_case_03_retail_picking.md](use_cases/use_case_03_retail_picking.md) |
| UC-04 | 语音家庭助手 | Interaction, Agent, UC, LC | [use_cases/use_case_04_voice_assistant.md](use_cases/use_case_04_voice_assistant.md) |
| UC-05 | 应急救援勘察 | LC(bipedal), Perception, PnC, DR | [use_cases/use_case_05_emergency_rescue.md](use_cases/use_case_05_emergency_rescue.md) |
| UC-06 | 产线质量检测 | Perception, UC, LC(wheeled) | [use_cases/use_case_06_quality_inspection.md](use_cases/use_case_06_quality_inspection.md) |
| UC-07 | 商超导览接待 | Interaction, LC(wheeled), UC, PnC | [use_cases/use_case_07_guided_tour.md](use_cases/use_case_07_guided_tour.md) |
| UC-08 | 实验室自动化 | UC, LC(wheeled), MS, Perception | [use_cases/use_case_08_lab_automation.md](use_cases/use_case_08_lab_automation.md) |
| UC-09 | 仓储物流分拣 | UC, LC(wheeled), PnC, Perception | [use_cases/use_case_09_warehouse_sorting.md](use_cases/use_case_09_warehouse_sorting.md) |
| UC-10 | 娱乐表演展示 | MP, UC, LC(bipedal) | [use_cases/use_case_10_entertainment.md](use_cases/use_case_10_entertainment.md) |
| UC-11 | 科研算法验证 | LC(bipedal), DR, Agent, TE | [use_cases/use_case_11_research_dev.md](use_cases/use_case_11_research_dev.md) |
| UC-12 | 自主建图与地图更新 | VSLAM, Lidar-SLAM, MapManager, LC | [use_cases/use_case_12_autonomous_mapping.md](use_cases/use_case_12_autonomous_mapping.md) |
| UC-13 | 遥操真机数采 | MS, UC, LC, DR, Interaction | [use_cases/use_case_13_teleop_data_collection.md](use_cases/use_case_13_teleop_data_collection.md) |

---

## 13. 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| v1.0 | 2026-05-06 | 初始版本，整合全部 27 个模块设计文档 |
