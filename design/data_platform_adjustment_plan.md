# 端侧软件架构调整方案：支撑物理AI数据服务平台

> 参考对象：智元机器人（觅蜂数据平台 + 部署态数据飞轮）、刻行空间（coScene端云协同数据平台）
> 目标：从"被动录制"升级为"主动采集-评估-上传-反馈"的数据飞轮闭环

---

## 一、参考架构核心特征

### 1.1 智元机器人：部署态数据飞轮

| 特征 | 说明 | 对端侧的启示 |
|------|------|-------------|
| **万台级真机部署** | 10000+台同型号机器人7×24小时产生数据 | 端侧必须具备**持续高频采集**能力，而非按需录制 |
| **虚实结合双轮驱动** | AgiBot World（真机）+ GenieSim（仿真） | 端侧需开放**仿真数据接口**，支持Sim2Real闭环 |
| **自主进化式采集** | 失效瞬间→自动接管→回退→人工接管→高价值数据捕获 | 运动控制层需支持**失效状态冻结与回退机制** |
| **觅蜂数据平台** | 数据交易、众包采集、全球数据生成基地 | Gateway需支持**数据资产化上传**，不仅是控制指令 |
| **多模态真机数据** | 视觉+LiDAR+力觉+触觉+关节状态+语言指令 | 端侧需强化**多模态精确时间同步**（<10ms） |
| **数据质量分层** | 1000万小时总量中仅200万小时为高质量 | 端侧需具备**数据质量实时评估与筛选**能力 |

### 1.2 刻行空间：coScene端云协同

| 特征 | 说明 | 对端侧的启示 |
|------|------|-------------|
| **coScout端侧助理** | 开箱即用，数据截取、压缩、上传 | 端侧需内置**轻量化数据管道**，低资源占用 |
| **规则引擎** | 云端定义触发条件→下发端侧→自动采集 | 需新增**端侧规则引擎模块**，解析云端采集策略 |
| **事件驱动采集** | 根据事件、告警、环境信息自动采集"有用数据" | DR需从"时间/指令触发"升级为"事件+价值双驱动" |
| **数据全生命周期** | 采集→净化→标记→评估→分析→可视化 | 端侧需支持**数据预处理与质量评估** |
| **数据容器引擎** | 非结构化数据高效存取、查询、治理 | 端侧数据存储需支持**结构化元数据索引** |

---

## 二、现有架构差距分析

### 2.1 差距矩阵

| 能力维度 | 现有状态 | 目标状态 | 差距等级 |
|---------|---------|---------|---------|
| 采集触发模式 | DR支持事件触发录制（故障前后30s） | 规则引擎驱动的事件+价值双驱动采集 | **高** |
| 数据质量评估 | 无实时评估，全部落盘 | 端侧实时评估数据价值，过滤冗余/脏数据 | **高** |
| 多模态时间同步 | Perception软同步50ms窗口 | <10ms硬同步，精确对齐视觉/LiDAR/力觉/关节 | **高** |
| 失效数据捕获 | MC切断电机即停止 | 失效瞬间冻结状态、回退、自动标注高价值片段 | **高** |
| 端云数据通道 | Gateway仅传控制指令/状态 | Gateway支持准实时数据上传、断点续传、加密 | **高** |
| 仿真数据接口 | 无 | 支持仿真数据注入、Sim2Real闭环 | **中** |
| 数据资产化 | DR数据本地存储，人工导出 | 自动切片、打标、压缩、上传云端数据平台 | **中** |
| 模型反馈闭环 | Agent/TE有任务反馈，无数据反馈 | 模型推理置信度低→自动触发针对性数据采集 | **中** |
| 数据预处理 | H.264/gzip压缩 | 智能降采样、去重、关键帧提取、语义压缩 | **低** |
| 众包/交易接口 | 无 | 数据脱敏、标准化、支持数据市场流通 | **低** |

### 2.2 关键问题

1. **DR是"黑匣子"不是"数据工厂"**：现有DR按需录制，无法支撑万台级7×24持续采集
2. **Gateway是"控制网关"不是"数据网关"**：数据上传能力薄弱，无断点续传、无压缩策略
3. **运动层是"控制器"不是"数据生产者"**：失效即停机，高价值数据瞬间丢失
4. **感知层是"实时融合"不是"训练数据对齐"**：50ms软同步无法满足VLA训练精度要求
5. **AI层是"任务执行"不是"数据策略生成"**：缺少"模型哪里不行→去哪里采数据"的闭环

---

## 三、调整方案（分模块）

### 3.1 新增模块

#### M1. Data Uploader（数据上传器）

**定位**：端侧到云端数据平台的专用上传通道，与Gateway解耦（Gateway专注控制指令，Data Uploader专注数据资产流转）。

**核心能力**：
- 断点续传、增量同步、压缩上传（支持H.265/AV1视频编码）
- 上传优先级队列：紧急故障数据优先，日常训练数据闲时上传
- 数据加密与脱敏（人脸/车牌/敏感场景自动模糊）
- 与云端数据平台（觅蜂/coScene）API对接

**包结构**：
```
data_uploader/
├── include/data_uploader/
│   ├── upload_manager.hpp      # 上传调度管理
│   ├── upload_queue.hpp        # 优先级队列
│   ├── compression_engine.hpp  # 压缩引擎
│   └── encryption.hpp          # 加密模块
├── src/
├── config/data_uploader_params.yaml
└── CMakeLists.txt
```

**接口**：
- Topic: `/data_uploader/upload_status` — 上传进度/状态
- Service: `TriggerUpload.srv` — 手动触发指定数据包上传
- Action: `UploadDataset.action` — 大批量数据集上传（含进度反馈）

---

#### M2. Data Rule Engine（端侧数据规则引擎）

**定位**：解析云端下发的采集规则，订阅Topic并触发DR/Data Uploader执行采集/上传。

**核心能力**：
- 规则热更新：云端通过Gateway下发规则YAML，端侧动态加载无需重启
- 多维度触发条件：Topic事件（如HDS告警）、状态条件（如SM进入FAULT）、时间窗口、随机采样
- 规则冲突仲裁：多条规则同时触发时的优先级处理
- 规则执行追踪：记录哪些规则触发了哪些数据采集，用于后续数据溯源

**规则示例**：
```yaml
rules:
  - name: "failure_capture"
    priority: 10
    trigger:
      topic: "/hds/diagnosis_event"
      condition: "level >= ERROR"
    action:
      type: "dr_record"
      duration: "before 10s, after 30s"
      tags: ["failure", "auto_capture"]
      upload: true

  - name: "vla_low_confidence"
    priority: 5
    trigger:
      topic: "/agent/decision_confidence"
      condition: "confidence < 0.3"
    action:
      type: "dr_record"
      duration: "before 5s, after 15s"
      tags: ["vla", "low_confidence"]
      upload: false  # 本地暂存，人工审核后上传
```

---

#### M3. Data Quality Filter（数据质量过滤器）

**定位**：实时评估传感器数据和VLA数据帧的质量，过滤无效/冗余数据，减少存储和上传开销。

**核心能力**：
- 传感器数据质量评分：图像模糊检测、LiDAR点云稀疏度、IMU噪声水平
- VLA数据帧完整性检查：图像+关节状态+语言指令是否齐全，时间戳是否对齐
- 冗余去重：相似场景（同一位置/姿态）降采样，保留关键变化帧
- 脏数据标记：传感器故障期间的数据自动标记为不可用

**部署位置**：作为DR内部的子模块，1kHz轻量级运行。

---

### 3.2 增强模块

#### E1. DR（Data Recorder）增强

| 增强项 | 现有能力 | 增强后能力 |
|-------|---------|-----------|
| 采集模式 | 手动/事件触发录制 | **规则引擎驱动** + 手动/事件触发 |
| 数据组织 | 按时间/任务分目录 | **按场景/标签/质量等级分层存储** |
| 元数据 | 基础标注（任务类型、场景标签） | **自动语义标注**（规则触发原因、数据质量评分、失效上下文） |
| 存储策略 | 循环录制，本地覆盖 | **智能生命周期管理**：高质量数据长期保留，低质量数据自动清理 |
| 输出格式 | ROS2 bag + VLA JSON | **增加云端标准格式**（支持直接导入觅蜂/coScene） |

**关键改动**：
- DR内部新增 `RuleSubscriber`：订阅Data Rule Engine的触发信号
- DR内部新增 `QualityScorer`：对接Data Quality Filter的评分结果
- DR内部新增 `MetadataEnricher`：自动附加规则触发原因、失效上下文、数据质量评分

---

#### E2. Gateway增强：数据资产出口

| 增强项 | 现有能力 | 增强后能力 |
|-------|---------|-----------|
| 上行通道 | 控制指令、状态上报、心跳 | **增加数据资产上行通道**（数据集、日志包、模型反馈） |
| 协议支持 | MQTT/HTTP for 控制 | **增加大文件传输协议**（分片上传、断点续传、压缩流） |
| 安全策略 | 控制指令签名验证 | **增加数据脱敏策略执行**（敏感区域自动模糊） |
| 带宽管理 | 控制指令优先 | **增加带宽自适应**：控制指令 > 故障数据 > 训练数据 |

**关键改动**：
- Gateway新增 `DataAssetUploader` 子模块，与Data Uploader模块对接
- Gateway配置新增 `data_upload` 段：带宽限制、上传时段、优先级策略

---

#### E3. MC（Motion Control）增强：失效即数据

| 增强项 | 现有能力 | 增强后能力 |
|-------|---------|-----------|
| 失效响应 | E-Stop→切断电机 | **E-Stop→冻结状态→回退→数据捕获** |
| 状态记录 | 无 | **失效前N周期完整状态快照**（关节状态、基座状态、控制指令、传感器读数） |
| 数据标注 | 无 | **自动标记失效类型**（根据HDS诊断结果或MC内部检测） |

**关键改动**：
- MC内部新增 `FailureCaptureBuffer`：环形缓冲区，持续保存最近N个控制周期的完整状态
- E-Stop路径增强：触发E-Stop时，先将FailureCaptureBuffer内容写入DR，再执行电机切断
- 恢复流程增强：从失效状态恢复时，支持"回退到失效前状态"重新尝试（类似智元的自主进化采集）

---

#### E4. Perception + HAL层增强：多模态硬同步

| 增强项 | 现有能力 | 增强后能力 |
|-------|---------|-----------|
| 时间同步 | 软同步50ms窗口 | **硬同步<10ms**，支持PTP/gPTP |
| 对齐维度 | 视觉+LiDAR | **视觉+LiDAR+力觉+触觉+关节状态+语言指令**六模态对齐 |
| 质量监控 | 传感器健康状态 | **逐帧质量评分**，实时上报DR |
| 数据输出 | 感知结果Topic | **增加原始数据对齐帧Topic**（供DR直接录制为VLA训练数据） |

**关键改动**：
- HAL_Sensor增加PTP时间同步支持
- Perception新增 `MultimodalSyncFrame`：一个消息包含所有模态的同步数据
- DR订阅 `MultimodalSyncFrame` 替代分别订阅各传感器Topic

---

#### E5. Agent/TE增强：模型驱动的数据策略

| 增强项 | 现有能力 | 增强后能力 |
|-------|---------|-----------|
| 任务反馈 | 任务成功/失败 | **增加决策置信度实时上报** |
| 数据采集 | 无主动策略 | **置信度低→自动触发针对性数据采集** |
| 数据利用 | 无 | **接收云端模型更新，评估端侧性能提升** |

**关键改动**：
- Agent新增 `ConfidenceMonitor`：监控VLA模型推理置信度
- Agent → Data Rule Engine：置信度低于阈值时，触发`vla_low_confidence`规则
- TE新增 `TaskDataTagger`：任务执行过程中自动标记关键场景（首次遇到、失败重试、人机协作）

---

### 3.3 新增接口（msg/srv/action）

#### data_uploader_msgs

```
msg/
  UploadStatus.msg       # 上传进度、速度、剩余数据量
  DataAssetInfo.msg      # 数据资产元数据（标签、质量评分、大小）
  UploadPriority.msg     # 上传优先级枚举

srv/
  TriggerUpload.srv      # 手动触发上传
  GetUploadQueue.srv     # 查询上传队列状态

action/
  UploadDataset.action   # 大批量数据集上传
```

#### data_rule_msgs

```
msg/
  RuleDefinition.msg     # 规则定义（触发条件+动作）
  RuleTriggerEvent.msg   # 规则触发事件
  RuleExecutionLog.msg   # 规则执行日志

srv/
  DeployRules.srv        # 云端下发规则
  GetActiveRules.srv     # 查询当前生效规则
```

#### dr_msgs 增强

```
msg/
  DataQualityScore.msg   # 数据质量评分（新增）
  FailureContext.msg     # 失效上下文（新增）
```

---

### 3.4 架构分层调整示意

```
┌─────────────────────────────────────────────────────────────────────────┐
│  AI 层      Agent (+ConfidenceMonitor)                                  │
│             TE   (+TaskDataTagger)                                      │
├─────────────────────────────────────────────────────────────────────────┤
│  应用层      FOTA    Setting    DR (+RuleSubscriber/QualityScorer)      │
│              RC     【Data Uploader】  【Data Rule Engine】              │
├─────────────────────────────────────────────────────────────────────────┤
│  感知/规划层 Perception (+MultimodalSyncFrame/硬同步)                   │
│             PnC      VSLAM    Lidar-SLAM                                │
├─────────────────────────────────────────────────────────────────────────┤
│  运动层      MC (+FailureCaptureBuffer)  UC  LC                         │
│             MP      MS                                                  │
├─────────────────────────────────────────────────────────────────────────┤
│  中间件层    SM    EM    Gateway (+DataAssetUploader)   HDS            │
├─────────────────────────────────────────────────────────────────────────┤
│  HAL层      HAL_EtherCAT  HAL_Camera  HAL_Lidar  HAL_Sensor (+PTP)     │
│             HAL_Audio     TF                                            │
└─────────────────────────────────────────────────────────────────────────┘
                                    ↑
                              数据资产上行通道
                                    ↑
┌─────────────────────────────────────────────────────────────────────────┐
│  云端        物理AI数据服务平台（觅蜂/coScene）                          │
│              - 数据容器引擎 / 工作流引擎 / 规则引擎                       │
│              - 数据标注平台 / 仿真平台 / 模型训练平台                     │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 四、关键流程调整

### 4.1 数据飞轮闭环流程（新增）

```
万台机器人部署运行
    ↓
【Data Rule Engine】持续监控：规则触发 → 通知DR采集
    ↓
【DR】采集多模态数据 → 【Data Quality Filter】实时评分 → 分层存储
    ↓
【Data Uploader】闲时/准实时上传 → 【Gateway】数据资产出口
    ↓
云端数据平台（觅蜂/coScene）接收 → 清洗/标注/训练
    ↓
新模型下发 → 【Gateway】接收 → 【Agent】加载新模型
    ↓
【Agent】推理置信度监控 → 置信度低 → 触发采集规则
    ↓
（循环）
```

### 4.2 失效即数据流程（新增）

```
MC检测到异常（或HDS上报CRITICAL）
    ↓
【MC】冻结FailureCaptureBuffer（最近N周期完整状态）
    ↓
【MC】回退到安全状态（如站立/停止）
    ↓
【MC】将FailureCaptureBuffer + 失效诊断信息 → DR
    ↓
【DR】自动标记为"high_value_failure_data"，质量评分=1.0
    ↓
【Data Rule Engine】触发failure_capture规则
    ↓
【Data Uploader】高优先级上传云端
    ↓
云端分析失效原因 → 生成针对性训练数据 → 模型迭代
```

### 4.3 虚实结合流程（新增）

```
云端仿真平台生成Sim数据
    ↓
【Gateway】接收仿真数据包
    ↓
【DR】存储仿真数据，标记为"sim_data"
    ↓
真机运行产生Real数据
    ↓
【DR】Real + Sim 数据混合组织
    ↓
【Data Uploader】上传混合数据集
    ↓
云端Sim2Real训练 → 模型下发 → 真机验证
```

---

## 五、实施优先级

| 优先级 | 模块 | 理由 |
|-------|------|------|
| **P0（立即）** | Data Uploader | 数据上云是飞轮的前提，无上传则无闭环 |
| **P0（立即）** | Data Rule Engine | 规则驱动采集是"主动采集"的核心 |
| **P0（立即）** | DR增强（RuleSubscriber/QualityScorer） | DR是数据源头，必须先改造 |
| **P1（3个月内）** | MC失效捕获增强 | 失效数据是高价值数据，智元核心能力 |
| **P1（3个月内）** | Perception硬同步增强 | VLA训练数据质量的关键 |
| **P1（3个月内）** | Gateway数据资产出口 | 与Data Uploader配合完成端云通道 |
| **P2（6个月内）** | Data Quality Filter | 减少冗余数据，降低存储和带宽成本 |
| **P2（6个月内）** | Agent/TE置信度反馈 | 模型驱动的数据策略，提升飞轮效率 |
| **P3（长期）** | 仿真数据接口 | Sim2Real闭环，虚实结合 |
| **P3（长期）** | 数据脱敏/交易接口 | 数据资产化、市场流通 |

---

## 六、风险与对策

| 风险 | 影响 | 对策 |
|------|------|------|
| 持续采集导致存储爆炸 | 端侧SSD快速耗尽 | Data Quality Filter + 智能生命周期管理，低质量数据自动清理 |
| 数据上传占用带宽 | 控制指令延迟增加 | Gateway带宽隔离策略：控制指令绝对优先，数据上传自适应降速 |
| 规则引擎过度触发 | DR频繁录制，影响实时性 | 规则冷却期 + 优先级仲裁 + 录制时长上限 |
| 失效捕获缓冲区溢出 | 关键状态丢失 | 环形缓冲区持续写入磁盘，内存仅保留索引 |
| 多模态硬同步复杂 | 硬件改造成本高 | 分阶段实施：先软件PTP，再硬件gPTP；先视觉+LiDAR，再扩展力觉 |

---

## 七、与现有设计文档的衔接

需要更新的设计文档：

1. `dr_design.md` — 增加 RuleSubscriber、QualityScorer、MetadataEnricher、智能生命周期管理
2. `gateway_design.md` — 增加 DataAssetUploader、带宽隔离策略
3. `mc_design.md` — 增加 FailureCaptureBuffer、E-Stop增强流程
4. `perception_design.md` — 增加 MultimodalSyncFrame、硬同步方案
5. `agent_design.md` / `te_design.md` — 增加 ConfidenceMonitor、TaskDataTagger
6. `system_architecture.md` — 增加 Data Uploader、Data Rule Engine、Data Quality Filter 三个新模块
7. 新增 `data_uploader_design.md`、`data_rule_engine_design.md`、`data_quality_filter_design.md`
