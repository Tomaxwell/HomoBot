# 人形机器人典型 Use Case 分析

> 本文档从宇树科技（Unitree）、智元机器人（AgiBot）、银河通用（GalaxyBot）等国内领先人形机器人企业的实际业务场景出发，选取 **15 个典型 Use Case**，逐案分析端侧各模块的协作方式、数据流向与安全约束。

---

## Use Case 清单

| 编号 | Use Case | 参考企业/场景 | 核心能力 | 文件 |
|------|----------|--------------|----------|------|
| UC-01 | [工业设施巡检](use_case_01_industrial_inspection.md) | 宇树 H1/G1（电力/石化巡检） | 自主导航 + 视觉检测 + 异常上报 | [→](use_case_01_industrial_inspection.md) |
| UC-02 | [工厂柔性装配](use_case_02_factory_assembly.md) | 智元远征（3C/汽车零部件装配） | 双臂精细操作 + 力控 + 视觉引导 | [→](use_case_02_factory_assembly.md) |
| UC-03 | [商超零售拣货](use_case_03_retail_picking.md) | 银河通用（货架拣货/补货） | 轮式移动 + 双臂抓取 + 库存核对 | [→](use_case_03_retail_picking.md) |
| UC-04 | [语音家庭助手](use_case_04_voice_assistant.md) | 通用场景（家庭服务） | 多轮对话 + VLA 决策 + 环境交互 | [→](use_case_04_voice_assistant.md) |
| UC-05 | [应急救援勘察](use_case_05_emergency_rescue.md) | 宇树 H1（灾害/消防场景） | 复杂地形行走 + 热成像 + 实时回传 | [→](use_case_05_emergency_rescue.md) |
| UC-06 | [产线质量检测](use_case_06_quality_inspection.md) | 智元远征（视觉质检） | 高精度视觉 + 缺陷分类 + 数据追溯 | [→](use_case_06_quality_inspection.md) |
| UC-07 | [商超导览接待](use_case_07_guided_tour.md) | 智元/通用（导购/展厅接待） | 人脸识别 + 语音交互 + 路径引导 | [→](use_case_07_guided_tour.md) |
| UC-08 | [实验室自动化](use_case_08_lab_automation.md) | 银河通用（生物/化学实验） | 精密移液 + 流程编排 + 安全合规 | [→](use_case_08_lab_automation.md) |
| UC-09 | [仓储物流分拣](use_case_09_warehouse_sorting.md) | 银河通用/智元（分拣搬运） | 大规模导航 + 物体识别 + 码垛 | [→](use_case_09_warehouse_sorting.md) |
| UC-10 | [娱乐表演展示](use_case_10_entertainment.md) | 宇树 G1（舞蹈/武术/春晚） | 动作播放 + 音乐同步 + 舞台安全 | [→](use_case_10_entertainment.md) |
| UC-11 | [科研算法验证](use_case_11_research_dev.md) | 宇树 H1/G1（学术/研发） | RL 策略训练 + 数据采集 + 快速迭代 | [→](use_case_11_research_dev.md) |
| UC-12 | [自主建图与地图更新](use_case_12_autonomous_mapping.md) | 通用（SLAM 建图） | 多传感器融合 + 地图管理 + 持久化 | [→](use_case_12_autonomous_mapping.md) |
| UC-13 | [遥操真机数采](use_case_13_teleop_data_collection.md) | 智元/宇树/Figure/特斯拉（数据闭环） | VR遥操 + 真机数据 + 自动标注 | [→](use_case_13_teleop_data_collection.md) |
| UC-14 | [建筑工地安全巡检](use_case_14_construction_safety_patrol.md) | 通用（智慧工地/安监） | 非结构化环境导航 + 安全合规检测 + 取证上报 | [→](use_case_14_construction_safety_patrol.md) |
| UC-15 | [康养跌倒响应与夜间巡护](use_case_15_eldercare_fall_response.md) | 通用（机构/居家康养） | 姿态异常检测 + 低打扰确认 + 告警闭环 + 隐私留存 | [→](use_case_15_eldercare_fall_response.md) |

---

## 阅读指南

每个 Use Case 文档包含以下结构：

1. **场景概述** — 业务背景、价值主张、典型客户
2. **时序图** — 模块间交互的完整时序（PlantUML/Mermaid 格式）
3. **模块协作矩阵** — 每个模块在该场景中的具体职责
4. **数据流向** — Topic / Service / Action 级别的数据流说明
5. **安全约束** — E-Stop 路径、SM 状态校验、故障处理
6. **关键参数** — 该场景下需要特别关注的配置项
7. **故障模式** — 典型故障及模块级联响应

---

## 模块全景速查

| 缩写 | 全称 | 层次 | 核心职责 |
|------|------|------|----------|
| SM | State Manager | 中间件层 | 全局状态机（17 状态），安全闸门 |
| EM | Executive Manager | 中间件层 | 进程生命周期治理 |
| Gateway | Gateway | 中间件层 | 云端/APP 通信唯一出口 |
| TE | Task Engine | 中间件层/AI 层 | 任务调度与执行 |
| HDS | Health Diagnosis System | 中间件层 | 故障诊断与定级 |
| Agent | Agent | AI 层 | VLA 具身智能体 |
| MC | Motion Control | 运动层 | 运动控制协调器（插件宿主）；加载 UC/LC 插件，聚合关节指令 |
| UC | Upper Body Control | 运动层 | 上肢控制插件（IK/轨迹规划/力控/夹爪） |
| LC | Lower Body Control | 运动层 | 下肢控制插件（足式: RL+WBC+步态；轮式: 底盘+升降柱） |
| MP | Motion Player | 运动层 | 预录动作序列播放 |
| MS | Motion Streamer | 运动层 | 运动指令流式整形 |
| PnC | Planning and Control | 感知/规划层 | 路径规划 + 行走控制 |
| Perception | Perception | 感知/规划层 | 视觉 + Lidar 感知融合 |
| VSLAM | Vision SLAM | 感知/规划层 | 视觉建图定位 |
| Lidar-SLAM | Lidar SLAM | 感知/规划层 | 激光雷达建图定位 |
| MapManager | Map Manager | 感知/规划层 | 地图管理 |
| DR | Data Recorder | 应用层 | 数据采集（VLA 训练数据） |
| FOTA | Firmware Over The Air | 应用层 | 固件升级 |
| Setting | Setting | 应用层 | 统一参数存储 |
| RC | Resource Collection | 应用层 | 日志聚合 + 性能监控 |
| HAL_EtherCAT | EtherCAT HAL | HAL & Infra | SOEM 主站，电机驱动 |
| HAL_Camera | Camera HAL | HAL & Infra | RealSense 相机管理（D435×1 + D405×2） |
| HAL_Lidar | Lidar HAL | HAL & Infra | Livox Mid-360s 固态激光雷达管理 |
| HAL_Sensor | Sensor HAL | HAL & Infra | IMU、Touch、环境传感器管理 |
| HAL_Audio | Audio HAL | HAL & Infra | 音频 + ASR + TTS |
| TF | TF Publisher | HAL & Infra | 坐标变换发布 |
| Interaction | Interaction | 交互层 | 多模态人机交互总控 |

---

## 按企业场景分类

### 宇树科技（Unitree）
- **H1/G1 工业巡检** → UC-01
- **应急救援勘察** → UC-05
- **娱乐表演展示** → UC-10
- **科研算法验证** → UC-11
- **遥操真机数采** → UC-13

### 智元机器人（AgiBot）
- **工厂柔性装配** → UC-02
- **产线质量检测** → UC-06
- **商超导览接待** → UC-07
- **仓储物流分拣** → UC-09
- **遥操真机数采** → UC-13

### 银河通用（GalaxyBot）
- **商超零售拣货** → UC-03
- **实验室自动化** → UC-08
- **仓储物流分拣** → UC-09

### Figure AI / 特斯拉 Optimus / 1X
- **遥操真机数采** → UC-13

### 通用场景
- **语音家庭助手** → UC-04
- **自主建图与地图更新** → UC-12
- **建筑工地安全巡检** → UC-14
- **康养跌倒响应与夜间巡护** → UC-15
