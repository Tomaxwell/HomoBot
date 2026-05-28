# 人形机器人端侧软件系统 — Claude Code 开发指南

## 文件职责

`CLAUDE.md` 是 Claude Code 在本仓库的主指令文件，Claude Code 应以本文件作为本地工作指南。

`AGENTS.md` 是 Codex CLI 的主指令文件。Claude Code 可以读取 `AGENTS.md` 了解共享架构约束和 Codex 产物边界，但不要改写 Codex 专属工作流，除非用户明确要求。

协作边界：
- 共享架构规则、模块清单、设计索引可以同时记录在 `CLAUDE.md` 和 `AGENTS.md`，但内容必须保持一致。
- Claude Code 专属流程写入 `CLAUDE.md` / `.claude/`。
- Codex 专属流程写入 `AGENTS.md` / `.codex/`。
- 两个工具不得覆盖对方前缀命名空间下的设计文档、包名、接口名和配置文件。

## 项目概述

本项目是人形机器人公司端侧软件系统，基于 ROS2 构建，包含 23 个核心模块，运行于机器人本体计算单元上。

## 系统架构

### 模块全景

| 缩写 | 全称 | 职责 |
|------|------|------|
| SM | State Manager | 机器人状态管理（全局状态机，17状态） |
| EM | Executive Manager | 进程生命周期治理（启动编排、故障恢复） |
| Gateway | Gateway | 端侧网关（云端/APP 通信唯一出口） |
| TE | Task Engine | 任务引擎（任务调度与执行，含VLA任务类型） |
| HDS | Health Diagnosis System | 健康监测（多维诊断、故障定级） |
| Agent | Agent | VLA具身智能体（LLM Agent + Skills + 记忆） |
| MC | Motion Control | 运动控制协调器（插件宿主，形态无关） |
| UC | Upper Body Control | 上肢运动控制（IK/轨迹规划/力控/夹爪，MC插件） |
| LC | Lower Body Control | 下肢运动控制（足式RL/WBC/步态；轮式底盘/升降柱，MC插件） |
| MP | Motion Player | 动作播放（预录动作序列） |
| MS | Motion Streamer | 动作流（VR/动捕服数据接入与运动重定向） |
| PnC | Planning and Control | 规划控制（路径规划 + 行走控制信号输出） |
| Perception | Perception | 感知融合（视觉 + Lidar） |
| VSLAM | Vision SLAM | 视觉建图定位（从HAL_Sensor接Camera） |
| Lidar-SLAM | Lidar SLAM | 激光雷达建图定位（从HAL_Sensor接Lidar） |
| MapManager | Map Manager | 地图管理 |
| DR | Data Recorder | 数据采集（含VLA训练数据管理） |
| FOTA | Firmware Over The Air | 固件升级 |
| Setting | Setting | 设置管理 |
| HealthMonitor | Health Monitor | 系统健康监控（资源/硬件/进程采集） |
| HAL_EtherCAT | EtherCAT HAL | SOEM主站，EtherCAT硬件抽象 |
| HAL_Camera | Camera HAL | RealSense相机管理（D435×1 + D405×2） |
| HAL_Lidar | Lidar HAL | Livox Mid-360s固态激光雷达管理 |
| HAL_Sensor | Sensor HAL | IMU、Touch、环境传感器管理 |
| HAL_Audio | Audio HAL | 音频硬件抽象（麦克风阵列/扬声器）、ASR、TTS |
| TF | TF Publisher | 读取URDF，发布坐标变换 |
| Interaction | Interaction | 多模态人机交互总控（语音/视觉/触觉/消息意图整合） |

### 分层架构

```
┌─────────────────────────────────────────────────────────────┐
│  AI 层        Agent, TE                                      │
├─────────────────────────────────────────────────────────────┤
│  交互层       Interaction                                    │
├─────────────────────────────────────────────────────────────┤
│  应用层       FOTA, Setting, DR                              │
│  中间件层     SM, EM, Gateway, HDS, HealthMonitor            │
├─────────────────────────────────────────────────────────────┤
│  感知/规划层  Perception, PnC, VSLAM, Lidar-SLAM, MapManager │
├─────────────────────────────────────────────────────────────┤
│  运动层       MC (协调器), UC (上肢), LC (下肢), MS, MP      │
├─────────────────────────────────────────────────────────────┤
│  中间件层     SM, EM, Gateway, HDS                           │
├─────────────────────────────────────────────────────────────┤
│  HAL & Infra  HAL_EtherCAT, HAL_Camera, HAL_Lidar, HAL_Sensor, HAL_Audio, TF, systemd│
└─────────────────────────────────────────────────────────────┘
```

### 功能域分组（6+1 架构映射）

当前分层架构按功能域自然映射，便于模块化开发和故障隔离：

| 功能域 | 对应模块 | 实时要求 | 安全等级 |
|--------|---------|---------|---------|
| **运动域** | MC, UC, LC, MS, MP, HAL_EtherCAT | 1kHz 硬实时 | ASIL-D |
| **感知域** | HAL_Camera, HAL_Lidar, HAL_Sensor, Perception, VSLAM, Lidar-SLAM | 传感器采集硬实时 | ASIL-B~D |
| **认知域** | Agent (VLA/LLM) | 50~200ms 软实时 | QM~ASIL-B |
| **交互域** | Interaction, HAL_Audio | 100~500ms 软实时 | QM |
| **任务域** | TE | 10~50Hz | ASIL-B |
| **平台域** | EM, Gateway, Setting, FOTA, DR, TF, HealthMonitor | 非实时 | QM~ASIL-B |

> 注：安全机制作为**跨域横切关注点**，由 SM（状态校验）、HDS（故障诊断）、MC（Safety Guardian）分别承担，不独立为单一域。

### Function Group State（功能域状态）

EM 支持按功能域分组管理进程启停。每个域有一组独立的 Function Group State，SM 通过域级状态切换实现任务级资源调度：

```
整机状态（SM）：ACTIVE
  ├─ motion_domain: {Idle, Balancing, Walking, Manipulating, Teleop_Upper, Teleop_Full}
  ├─ perception_domain: {Idle, Active, SLAM}
  ├─ cognition_domain: {Idle, Active}
  ├─ interaction_domain: {Idle, Listening, Speaking, Interactive}
  └─ task_domain: {Idle, Executing, Paused}
```

**部分身体遥操示例**：
- SM 设置 `motion_domain = Teleop_Upper`：MS 控制上肢，LC 插件继续下肢平衡
- SM 设置 `motion_domain = Teleop_Full`：MS 控制全身关节
- TE 请求切换前，通过 EM 确认目标域的进程资源已分配

### 资源隔离原则

EM 在进程启动时通过 Execution Manifest 配置资源隔离：

```yaml
# em_manifest.yaml 示例
motion_control:
  cpuset: [2, 3]        # CPU 核心隔离
  priority: 99          # SCHED_FIFO 实时优先级
  mem_limit: 512M
cognition:
  cpuset: [4, 5, 6, 7]  # GPU 任务容忍调度
  gpu_limit: 80%
```

### 时间同步服务

平台域提供统一传感器时间同步（gPTP/PTP），由 HAL_Sensor 作为时间主节点分发，HAL_Camera、HAL_Lidar 启动时注册同步。

### 核心通信模式

- **ROS2 Topic**：高频传感器数据、状态广播（`/sm/robot_state`）
- **ROS2 Service**：同步请求/响应（状态转换、参数查询）
- **ROS2 Action**：长耗时操作（导航、动作播放，含进度反馈）
- **MQTT Broker**：跨语言进程间通信（EM ↔ HDS）

## 开发规范

### 设计原则

1. **模块职责单一** — 每个模块有清晰的职责边界，参考上表
2. **接口先行** — 先定义 ROS2 msg/srv/action，再实现逻辑
3. **安全优先** — 运动相关模块必须有急停路径（E-Stop），且优先级最高
4. **可观测性** — 每个模块必须有心跳上报和健康状态接口
5. **故障隔离** — 单模块故障不能级联导致系统崩溃，EM 负责熔断

### 文件命名约定

- ROS2 消息：`ModuleName.msg`，如 `RobotState.msg`
- ROS2 服务：`VerbNoun.srv`，如 `RequestTransition.srv`
- ROS2 Action：`VerbNoun.action`，如 `TransitionTo.action`
- 参数文件：`{module}_params.yaml`
- Launch 文件：`{module}.launch.py`

### 并行协作边界

本项目允许 Codex 与 Claude Code 并行设计模块。两个工具使用统一的命名空间，通过以下机制避免冲突：

- 修改共享设计索引时只追加或更新自己负责的条目；发现对方新增条目时保持原样
- Codex 专属流程写入 `AGENTS.md` / `.codex/`；Claude Code 专属流程写入 `CLAUDE.md` / `.claude/`

### 统一模块命名

- 设计文档：`{module}_design.md`
- 接口包：`{module}_msgs`
- 实现包：`{module}`
- 节点名：`{module}_node`
- 自定义 Topic/Service/Action 命名空间：`/{module}/...`
- 参数文件：`{module}_params.yaml`
- Launch 文件：`{module}.launch.py`

例外：ROS2 标准生态接口保持原名，例如 `/tf`、`/tf_static`、`/joint_states`、`robot_description`。

### 包结构约定

每个模块包含两个 ROS2 包：
```
{module}_msgs/    # 消息定义包（纯接口）
{module}/         # 节点实现包
├── include/{module}/
├── src/
├── config/
├── launch/
└── CMakeLists.txt
```

### 设计文档约定

设计文档保存为 `{module}_design.md`，包含：
1. 模块概述与定位
2. 职责边界
3. 状态机（如适用）
4. ROS2 接口定义（msg/srv/action/topic/service/action 汇总）
5. 内部设计（节点结构、关键流程）
6. 与其他模块的交互
7. 关键参数与配置
8. 错误码定义
9. 包结构

## 已完成设计

### Claude Code 设计
- [Interaction Cloud](design/layer_00_cloud/interaction_cloud_design.md) — 交互云架构设计（CloudLLM/RAG/知识库/多模态云服务）

- [Agent](design/layer_01_ai/agent_design.md) — VLA具身智能体（LLM Agent + Skills + 记忆）
- [Task Engine (TE)](design/layer_01_ai/te_design.md) — 任务调度与执行（含VLA任务类型）
- [Interaction](design/layer_02_interaction/interaction_design.md) — 多模态人机交互总控
- [FOTA](design/layer_03_application/fota_design.md) — 固件升级（下载/验证/安装/回滚）
- [Setting](design/layer_03_application/setting_design.md) — 设置管理（统一参数存储+Schema验证+热更新）
- [Data Recorder (DR)](design/layer_03_application/dr_design.md) — 数据采集（VLA训练数据+故障黑匣子）
- [Health Monitor](design/layer_06_middleware/health_monitor_design.md) — 系统健康监控（CPU/内存/磁盘/电池/温度/网络/GPU/进程/急停采集）
- [Perception](design/layer_04_perception_planning/perception_design.md) — 感知融合（视觉+Lidar多传感器融合）
- [Planning and Control (PnC)](design/layer_04_perception_planning/pnc_design.md) — 规划控制（路径规划 + 行走控制信号输出）
- [Vision SLAM (VSLAM)](design/layer_04_perception_planning/vslam_design.md) — 视觉建图定位（特征提取+VO+回环检测）
- [Lidar SLAM](design/layer_04_perception_planning/lidar_slam_design.md) — 激光雷达建图定位（扫描匹配+占据栅格）
- [Map Manager](design/layer_04_perception_planning/mapmanager_design.md) — 地图管理（存储/加载/切换/生命周期）
- [Motion Control (MC)](design/layer_05_motion/mc_design.md) — 运动控制协调器（插件宿主，形态无关）
- [Upper Body Control (UC)](design/layer_05_motion/uc_design.md) — 上肢运动控制（IK/轨迹规划/末端力控/夹爪，MC插件）
- [Lower Body Control (LC)](design/layer_05_motion/lc_design.md) — 下肢运动控制（足式RL/WBC/步态；轮式底盘/升降柱，MC插件）
- [Motion Player (MP)](design/layer_05_motion/mp_design.md) — 动作播放（预录动作序列执行）
- [Motion Streamer (MS)](design/layer_05_motion/ms_design.md) — 动作流（VR/动捕服数据接入与运动重定向）
- [State Manager (SM)](design/layer_06_middleware/sm_design.md) — 全局状态机（17状态，足式人形模式扩充）
- [Executive Manager (EM)](design/layer_06_middleware/em_design_v2.md) — 进程生命周期治理
- [Gateway](design/layer_06_middleware/gateway_design.md) — 端侧网关（云端/APP通信唯一出口）
- [Health Diagnosis System (HDS)](design/layer_06_middleware/hds_design.md) — 健康监测（多维诊断、故障定级，单SOC原始设计）
- [HDS Master](design/layer_06_middleware/hds_master_design.md) — HDS 主节点（全局定级权威、Slave管理、跨SOC关联诊断、Bridge协议服务端）
- [HDS Slave](design/layer_06_middleware/hds_slave_design.md) — HDS 从节点（本地采集代理、自治安全守护、Bridge协议客户端）
- [EtherCAT HAL](design/layer_07_hal_infra/hal_ethercat_design.md) — SOEM主站，EtherCAT硬件抽象
- [HAL Camera](design/layer_07_hal_infra/hal_camera_design.md) — RealSense相机管理（D435×1 + D405×2）
- [HAL Lidar](design/layer_07_hal_infra/hal_lidar_design.md) — Livox Mid-360s固态激光雷达管理
- [HAL Sensor](design/layer_07_hal_infra/hal_sensor_design.md) — IMU、Touch、环境传感器管理
- [HAL Audio](design/layer_07_hal_infra/hal_audio_design.md) — 音频硬件抽象+ASR+TTS
- [TF Publisher (TF)](design/layer_07_hal_infra/tf_design.md) — 坐标变换发布（URDF解析 + /tf + /tf_static）

## Claude 行为约束

### 任务前检查清单

- 设计新模块前，先查阅本文件确认模块边界，避免职责重叠
- 涉及运动控制（MC/MS/MP/PnC）的设计或代码，必须确认 E-Stop 路径和 SM 状态校验
- 修改 ROS2 接口前，检查交互矩阵中的依赖模块是否受影响
- 需要查询 ROS2 / C++ API 时，使用 `use context7` 获取实时文档

### 已知错误（禁止重犯）

- **禁止**在非 EM 模块中直接启动/停止其他进程（只能通过 EM 的控制接口）
- **禁止**在 MC/MS/MP 的运动回调中调用阻塞式 ROS2 Service（用异步调用或本地状态缓存）
- **禁止**跳过 `/sm/is_motion_allowed` 校验直接下发运动指令
- **禁止**在模块内部做故障定级决策（只上报原始数据给 HDS）
- **禁止**非 Gateway 模块直接调用云端 API

### 输出约定

- 设计文档必须包含完整的 10 节结构（参考 em_design_v2.md / sm_design.md）
- msg 字段名用 `snake_case`，消息类型名用 `PascalCase`
- 每个设计文档完成后，在本文件"已完成设计"列表中追加记录
- 实现代码提交前必须通过 `/review` 命令审查

## 工作流指南

- 新模块设计：使用 `/module-design <模块缩写>` 命令
- ROS2 接口定义：使用 `/ros2-interface <模块缩写>` 命令
- 代码审查：使用 `/review` 命令
- 设计审查（安全/架构）：使用 `/design-review` 命令

## 重要约束

- 所有运动指令必须经过 SM 状态校验，`FAULT` 或 `ACTIVE_E_STOP` 状态下拒绝
- EM 是唯一有权启动/停止其他模块进程的组件
- Gateway 是唯一的云端通信出口，不允许其他模块直接访问云端 API
- HDS 负责故障定级，其他模块只上报原始数据，不做业务决策
- 设计文档中的图表全部都需要设计成mermaid图表。
