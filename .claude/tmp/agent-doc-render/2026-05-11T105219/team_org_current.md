# 小人形机器人端侧软件与语音交互团队人力阵型

## 一、概述

本文档基于当前端侧软件架构（23个核心模块，ROS2分布式架构），为**小型人形机器人项目**提供端侧软件团队和语音交互团队的人力阵型建议。

**目标团队规模**：

- Phase 1（MVP阶段，0-6个月）：端侧软件 8-10人 + 语音交互 3-4人
- Phase 2（量产阶段，6-12个月）：端侧软件 12-15人 + 语音交互 5-6人

---

## 二、端侧软件团队人力阵型

### 2.1 组织架构
```plaintext
端侧软件负责人（1人）
├── 系统架构组（2-3人）
│   ├── 中间件工程师：SM / EM / Gateway / HDS
│   └── 系统工程师：BSP / 启动流程 / 系统监控
├── 运动控制组（3-4人）
│   ├── 运控算法工程师：MC / UC / LC 协调与插件开发
│   ├── 轨迹规划工程师：MS / MP / 动作序列
│   └── 步态/平衡工程师：PnC / WBC / RL策略
├── 感知规划组（2-3人）
│   ├── SLAM工程师：VSLAM / Lidar-SLAM / MapManager
│   └── 感知融合工程师：Perception / 多传感器标定
├── AI与交互组（2-3人）
│   ├── Agent工程师：LLM Agent / Skill Registry / Memory
│   ├── 任务引擎工程师：TE / 任务调度 / VLA任务
│   └── 交互工程师：Interaction / 多模态融合
└── HAL与基础设施组（2人）
    ├── HAL工程师：EtherCAT / Camera / Lidar / Sensor / Audio
    └── 工具链/DevOps工程师：FOTA / DR / RC / CI/CD

```

### 2.2 角色定义与职责

<lark-table rows="14" cols="3" header-row="true" column-widths="244,244,244">

  <lark-tr>
    <lark-td>
      角色
    </lark-td>
    <lark-td>
      核心职责
    </lark-td>
    <lark-td>
      对应模块
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **端侧软件负责人**
    </lark-td>
    <lark-td>
      技术决策、跨组协调、架构把关、风险管控
    </lark-td>
    <lark-td>
      全局
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **中间件工程师**
    </lark-td>
    <lark-td>
      状态机实现、进程生命周期管理、云端通信、健康诊断
    </lark-td>
    <lark-td>
      SM / EM / Gateway / HDS
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **系统工程师**
    </lark-td>
    <lark-td>
      BSP适配、启动时序、资源监控、系统安全
    </lark-td>
    <lark-td>
      systemd / 内核 / 启动流程
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **运控算法工程师**
    </lark-td>
    <lark-td>
      MC插件开发、上肢IK/力控、下肢RL/WBC、形态适配
    </lark-td>
    <lark-td>
      MC / UC / LC
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **轨迹规划工程师**
    </lark-td>
    <lark-td>
      动作流式调度、预录动作执行、关节轨迹规划
    </lark-td>
    <lark-td>
      MS / MP
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **步态/平衡工程师**
    </lark-td>
    <lark-td>
      行走控制信号生成、步态生成器、接触力规划
    </lark-td>
    <lark-td>
      PnC
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **SLAM工程师**
    </lark-td>
    <lark-td>
      视觉/激光SLAM、地图管理、定位算法
    </lark-td>
    <lark-td>
      VSLAM / Lidar-SLAM / MapManager
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **感知融合工程师**
    </lark-td>
    <lark-td>
      多传感器融合、目标检测跟踪、传感器标定
    </lark-td>
    <lark-td>
      Perception
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **Agent工程师**
    </lark-td>
    <lark-td>
      LLM Agent架构、Skill系统、记忆管理、上下文构建
    </lark-td>
    <lark-td>
      Agent
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **任务引擎工程师**
    </lark-td>
    <lark-td>
      任务调度器、VLA任务类型支持、任务状态机
    </lark-td>
    <lark-td>
      TE
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **交互工程师**
    </lark-td>
    <lark-td>
      多模态输入融合、意图理解、交互状态机
    </lark-td>
    <lark-td>
      Interaction
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **HAL工程师**
    </lark-td>
    <lark-td>
      硬件抽象层开发、驱动集成、设备管理
    </lark-td>
    <lark-td>
      HAL_EtherCAT / HAL_Camera / HAL_Lidar / HAL_Sensor / HAL_Audio
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **工具链/DevOps工程师**
    </lark-td>
    <lark-td>
      固件升级、数据采集、日志聚合、CI/CD流水线
    </lark-td>
    <lark-td>
      FOTA / DR / RC
    </lark-td>
  </lark-tr>
</lark-table>

### 2.3 Phase 1 人力配置（MVP，0-6个月）

**合计：8-10人**

<lark-table rows="10" cols="4" header-row="true" column-widths="183,183,183,183">

  <lark-tr>
    <lark-td>
      角色
    </lark-td>
    <lark-td>
      人数
    </lark-td>
    <lark-td>
      优先级
    </lark-td>
    <lark-td>
      说明
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      端侧软件负责人
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      P0
    </lark-td>
    <lark-td>
      必须首位到位，负责技术基线
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      中间件工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      P0
    </lark-td>
    <lark-td>
      SM/EM/Gateway/HDS是系统骨架
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      HAL工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      P0
    </lark-td>
    <lark-td>
      硬件驱动层，所有上层依赖
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      运控算法工程师
    </lark-td>
    <lark-td>
      2
    </lark-td>
    <lark-td>
      P0
    </lark-td>
    <lark-td>
      小人形核心能力，UC+LC各1人
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      步态/平衡工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      P1
    </lark-td>
    <lark-td>
      与运控密切配合，初期可合并
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      SLAM工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      P1
    </lark-td>
    <lark-td>
      导航基础，初期VSLAM优先
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      Agent工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      P1
    </lark-td>
    <lark-td>
      智能体Demo能力
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      交互工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      P2
    </lark-td>
    <lark-td>
      初期可由Agent工程师兼任
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      工具链/DevOps工程师
    </lark-td>
    <lark-td>
      0.5
    </lark-td>
    <lark-td>
      P2
    </lark-td>
    <lark-td>
      与其他项目共享或后期补充
    </lark-td>
  </lark-tr>
</lark-table>

**Phase 1 重点**：

- 搭建完整的ROS2通信骨架（SM/EM/Gateway）
- 实现基础运动能力（站立、行走、手臂动作）
- 完成核心HAL层（EtherCAT + Camera + Lidar）
- 跑通端到端Demo：语音指令 -> Agent理解 -> 任务调度 -> 运动执行

### 2.4 Phase 2 人力配置（量产，6-12个月）

**合计：12-15人**

<lark-table rows="14" cols="3" header-row="true" column-widths="244,244,244">

  <lark-tr>
    <lark-td>
      角色
    </lark-td>
    <lark-td>
      人数
    </lark-td>
    <lark-td>
      新增说明
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      端侧软件负责人
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      -
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      中间件工程师
    </lark-td>
    <lark-td>
      2
    </lark-td>
    <lark-td>
      拆分：1人专注SM/EM，1人专注Gateway/HDS
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      系统工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      新增：量产级BSP、OTA安全、启动优化
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      HAL工程师
    </lark-td>
    <lark-td>
      2
    </lark-td>
    <lark-td>
      拆分：1人专注运动相关HAL（EtherCAT/Sensor），1人专注感知相关HAL（Camera/Lidar/Audio）
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      运控算法工程师
    </lark-td>
    <lark-td>
      2
    </lark-td>
    <lark-td>
      维持，UC和LC专人负责
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      轨迹规划工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      新增：从运控拆分，专注MS/MP精细动作
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      步态/平衡工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      -
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      SLAM工程师
    </lark-td>
    <lark-td>
      2
    </lark-td>
    <lark-td>
      新增：VSLAM和Lidar-SLAM各1人
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      感知融合工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      新增：从SLAM拆分
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      Agent工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      -
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      任务引擎工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      新增：从Agent拆分，专注复杂任务编排
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      交互工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      专职：脱离Agent独立
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      工具链/DevOps工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      全职：量产工具链、数据闭环
    </lark-td>
  </lark-tr>
</lark-table>

**Phase 2 重点**：

- 运动性能优化（动态平衡、抗扰动）
- 多传感器融合感知上线
- VLA任务端到端闭环
- 量产工具链（FOTA自动升级、数据自动采集）
- 系统稳定性（故障诊断、自动恢复）

### 2.5 技能要求矩阵

<lark-table rows="14" cols="3" header-row="true" column-widths="244,244,244">

  <lark-tr>
    <lark-td>
      角色
    </lark-td>
    <lark-td>
      必备技能
    </lark-td>
    <lark-td>
      加分技能
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      端侧软件负责人
    </lark-td>
    <lark-td>
      ROS2架构、C++、实时系统、团队管理
    </lark-td>
    <lark-td>
      人形机器人背景、控制理论、AI系统
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      中间件工程师
    </lark-td>
    <lark-td>
      ROS2（topic/service/action）、C++17、状态机设计
    </lark-td>
    <lark-td>
      DDS调优、安全通信、MQTT
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      系统工程师
    </lark-td>
    <lark-td>
      Linux BSP、Yocto/Buildroot、内核裁剪
    </lark-td>
    <lark-td>
      ARM SoC、实时补丁、Secure Boot
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      HAL工程师
    </lark-td>
    <lark-td>
      设备驱动开发、C++、硬件协议（EtherCAT/USB/I2C/SPI）
    </lark-td>
    <lark-td>
      ROS2 hardware_interface、RealSense SDK
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      运控算法工程师
    </lark-td>
    <lark-td>
      机器人学、IK、轨迹规划、C++、控制理论
    </lark-td>
    <lark-td>
      RL（强化学习）、WBC、Mujoco/Isaac Sim
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      轨迹规划工程师
    </lark-td>
    <lark-td>
      运动学、样条插值、ROS2 action、C++
    </lark-td>
    <lark-td>
      时间最优轨迹、碰撞检测
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      步态/平衡工程师
    </lark-td>
    <lark-td>
      动力学、WBC、MPC、足式机器人
    </lark-td>
    <lark-td>
      RL策略训练、Sim-to-real
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      SLAM工程师
    </lark-td>
    <lark-td>
      视觉/Lidar SLAM、OpenCV/PCL、C++
    </lark-td>
    <lark-td>
      回环检测、多传感器融合定位
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      感知融合工程师
    </lark-td>
    <lark-td>
      深度学习推理部署、TensorRT、多传感器标定
    </lark-td>
    <lark-td>
      3D目标检测、点云分割
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      Agent工程师
    </lark-td>
    <lark-td>
      LLM应用开发、Python、Prompt Engineering、向量数据库
    </lark-td>
    <lark-td>
      VLA模型、LangChain/LangGraph、RAG
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      任务引擎工程师
    </lark-td>
    <lark-td>
      ROS2 action、状态机、Python/C++、任务规划
    </lark-td>
    <lark-td>
      行为树、HTN规划
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      交互工程师
    </lark-td>
    <lark-td>
      多模态融合、信号处理、Python、ROS2
    </lark-td>
    <lark-td>
      语音交互、NLU、情感计算
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      工具链/DevOps工程师
    </lark-td>
    <lark-td>
      CI/CD、Docker、Bazel/CMake、Python
    </lark-td>
    <lark-td>
      嵌入式OTA、数据管道、MLflow
    </lark-td>
  </lark-tr>
</lark-table>

---

## 三、语音交互团队人力阵型

### 3.1 组织架构
```plaintext
语音交互负责人（1人，可由端侧软件负责人兼任或独立）
├── 音频前端组（1-2人）
│   ├── 声学工程师：麦克风阵列设计、声学结构
│   └── 音频算法工程师：AEC/NS/BF/VAD/AGC
├── ASR算法组（1-2人）
│   ├── 本地ASR工程师：KWS唤醒、本地命令词识别
│   └── 云端ASR对接工程师：云端ASR协议、流式传输、弱网适配
├── TTS算法组（1人）
│   └── TTS工程师：本地VITS部署、云端TTS对接、音色管理
└── 多模态交互算法组（1-2人）
    ├── 唇形检测工程师：视觉唇形识别、音视同步
    ├── 声纹识别工程师：声纹特征提取、说话人分离
    └── 手势识别工程师：视觉手势检测、交互意图映射

```

### 3.2 角色定义与职责

<lark-table rows="10" cols="3" header-row="true" column-widths="244,244,244">

  <lark-tr>
    <lark-td>
      角色
    </lark-td>
    <lark-td>
      核心职责
    </lark-td>
    <lark-td>
      对应模块/接口
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **语音交互负责人**
    </lark-td>
    <lark-td>
      语音交互整体技术路线、供应商对接（科大讯飞等）、团队协调
    </lark-td>
    <lark-td>
      HAL_Audio / Interaction
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **声学工程师**
    </lark-td>
    <lark-td>
      麦克风阵列选型/布局、腔体声学设计、噪声环境测试
    </lark-td>
    <lark-td>
      HAL_Audio硬件层
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **音频算法工程师**
    </lark-td>
    <lark-td>
      AEC回声消除、NS噪声抑制、BF波束形成、VAD语音检测、AGC自动增益
    </lark-td>
    <lark-td>
      HAL_Audio Pipeline
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **本地ASR工程师**
    </lark-td>
    <lark-td>
      唤醒词模型训练/部署、本地命令词识别、低功耗优化
    </lark-td>
    <lark-td>
      HAL_Audio ASR Engine
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **云端ASR对接工程师**
    </lark-td>
    <lark-td>
      科大讯飞AIUI/云ASR协议对接、流式RPC、网络降级策略
    </lark-td>
    <lark-td>
      HAL_Audio ASR Engine
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **TTS工程师**
    </lark-td>
    <lark-td>
      VITS本地模型部署优化、云端TTS对接、多音色管理、流式播放
    </lark-td>
    <lark-td>
      HAL_Audio TTS Engine
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **唇形检测工程师**
    </lark-td>
    <lark-td>
      唇部关键点检测、唇动-音频同步、视觉VAD增强
    </lark-td>
    <lark-td>
      Interaction多模态融合
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **声纹识别工程师**
    </lark-td>
    <lark-td>
      声纹特征提取、说话人分离/识别、声纹库管理
    </lark-td>
    <lark-td>
      Interaction身份识别
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **手势识别工程师**
    </lark-td>
    <lark-td>
      手势检测分类、手势-语音意图融合、交互触发
    </lark-td>
    <lark-td>
      Interaction多模态融合
    </lark-td>
  </lark-tr>
</lark-table>

### 3.3 Phase 1 人力配置（MVP，0-6个月）

**合计：3-4人**

<lark-table rows="6" cols="4" header-row="true" column-widths="183,183,183,183">

  <lark-tr>
    <lark-td>
      角色
    </lark-td>
    <lark-td>
      人数
    </lark-td>
    <lark-td>
      优先级
    </lark-td>
    <lark-td>
      说明
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      语音交互负责人
    </lark-td>
    <lark-td>
      0.5
    </lark-td>
    <lark-td>
      P0
    </lark-td>
    <lark-td>
      可由端侧软件负责人兼任
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      音频算法工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      P0
    </lark-td>
    <lark-td>
      全栈音频前端（AEC/NS/BF/VAD）
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      本地ASR工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      P0
    </lark-td>
    <lark-td>
      唤醒词+本地命令词，基础ASR能力
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      TTS工程师
    </lark-td>
    <lark-td>
      0.5
    </lark-td>
    <lark-td>
      P1
    </lark-td>
    <lark-td>
      初期可用云端TTS，本地VITS后期补充
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      云端ASR对接工程师
    </lark-td>
    <lark-td>
      0.5
    </lark-td>
    <lark-td>
      P1
    </lark-td>
    <lark-td>
      与本地ASR工程师可合并
    </lark-td>
  </lark-tr>
</lark-table>

**Phase 1 重点**：

- 基础音频前端 Pipeline 跑通（AEC + NS + VAD）
- 唤醒词识别（本地KWS）
- 云端ASR对接（科大讯飞AIUI）
- 云端TTS对接（ fallback 策略）
- 端到端语音交互Demo：唤醒 -> ASR -> 意图 -> TTS响应

### 3.4 Phase 2 人力配置（量产，6-12个月）

**合计：5-6人**

<lark-table rows="8" cols="3" header-row="true" column-widths="244,244,244">

  <lark-tr>
    <lark-td>
      角色
    </lark-td>
    <lark-td>
      人数
    </lark-td>
    <lark-td>
      新增说明
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      语音交互负责人
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      独立专职
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      声学工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      新增：量产声学结构优化、产线标定
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      音频算法工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      -
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      本地ASR工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      -
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      云端ASR对接工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      独立：深度优化弱网、多供应商适配
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      TTS工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      -
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      多模态交互算法工程师
    </lark-td>
    <lark-td>
      1
    </lark-td>
    <lark-td>
      新增：唇形/声纹/手势三选一或全栈
    </lark-td>
  </lark-tr>
</lark-table>

**Phase 2 重点**：

- 本地VITS TTS部署（降低延迟、支持离线）
- 声纹识别上线（区分说话人、个性化响应）
- 唇形检测辅助VAD（嘈杂环境提升识别率）
- 手势识别（补充非语音交互通道）
- 产线音频标定工具（保证量产一致性）

### 3.5 技能要求矩阵

<lark-table rows="8" cols="3" header-row="true" column-widths="244,244,244">

  <lark-tr>
    <lark-td>
      角色
    </lark-td>
    <lark-td>
      必备技能
    </lark-td>
    <lark-td>
      加分技能
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      语音交互负责人
    </lark-td>
    <lark-td>
      语音交互系统架构、供应商管理、项目管理
    </lark-td>
    <lark-td>
      科大讯飞AIUI、人机交互设计
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      声学工程师
    </lark-td>
    <lark-td>
      声学测量、麦克风阵列原理、ANSYS/COM声学仿真
    </lark-td>
    <lark-td>
      机器人结构集成、产线标定
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      音频算法工程师
    </lark-td>
    <lark-td>
      数字信号处理（DSP）、webrtc音频处理、C/C++
    </lark-td>
    <lark-td>
      TensorFlow Lite MCU、NEON优化
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      本地ASR工程师
    </lark-td>
    <lark-td>
      语音识别、KWS、嵌入式模型部署（TFLite/ONNX）
    </lark-td>
    <lark-td>
      端到端ASR（Conformer/Whisper）、量化压缩
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      云端ASR对接工程师
    </lark-td>
    <lark-td>
      gRPC/WebSocket流式通信、协议设计、网络编程
    </lark-td>
    <lark-td>
      科大讯飞MSC/AIUI SDK、多Region容灾
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      TTS工程师
    </lark-td>
    <lark-td>
      语音合成（VITS/Tacotron）、声码器、模型推理优化
    </lark-td>
    <lark-td>
      voice conversion、情感TTS
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      多模态交互算法工程师
    </lark-td>
    <lark-td>
      计算机视觉（人脸/手势）、深度学习部署、Python/C++
    </lark-td>
    <lark-td>
      MediaPipe、OpenVINO、多模态融合算法
    </lark-td>
  </lark-tr>
</lark-table>

---

## 四、团队间协作模式

### 4.1 接口契约

<lark-table rows="8" cols="4" header-row="true" column-widths="183,183,183,183">

  <lark-tr>
    <lark-td>
      协作边界
    </lark-td>
    <lark-td>
      上游（输出方）
    </lark-td>
    <lark-td>
      下游（消费方）
    </lark-td>
    <lark-td>
      接口形式
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      音频数据流
    </lark-td>
    <lark-td>
      HAL_Audio
    </lark-td>
    <lark-td>
      Interaction
    </lark-td>
    <lark-td>
      ROS2 Topic `/hal_audio/vad_event`
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      语音识别结果
    </lark-td>
    <lark-td>
      HAL_Audio
    </lark-td>
    <lark-td>
      Interaction
    </lark-td>
    <lark-td>
      ROS2 Topic `/hal_audio/speech_recognition_result`
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      TTS播放
    </lark-td>
    <lark-td>
      Interaction
    </lark-td>
    <lark-td>
      HAL_Audio
    </lark-td>
    <lark-td>
      ROS2 Action `/hal_audio/speak`
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      交互意图
    </lark-td>
    <lark-td>
      Interaction
    </lark-td>
    <lark-td>
      Agent / TE
    </lark-td>
    <lark-td>
      ROS2 Topic `/interaction/interaction_event`
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      任务执行
    </lark-td>
    <lark-td>
      TE
    </lark-td>
    <lark-td>
      MC / MS / MP
    </lark-td>
    <lark-td>
      ROS2 Action
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      运动状态
    </lark-td>
    <lark-td>
      MC
    </lark-td>
    <lark-td>
      SM
    </lark-td>
    <lark-td>
      ROS2 Topic `/sm/robot_state`
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      健康状态
    </lark-td>
    <lark-td>
      HDS
    </lark-td>
    <lark-td>
      EM / Gateway
    </lark-td>
    <lark-td>
      MQTT / ROS2 Topic
    </lark-td>
  </lark-tr>
</lark-table>

### 4.2 日常协作机制

<lark-table rows="5" cols="4" header-row="true" column-widths="183,183,183,183">

  <lark-tr>
    <lark-td>
      机制
    </lark-td>
    <lark-td>
      频率
    </lark-td>
    <lark-td>
      参与方
    </lark-td>
    <lark-td>
      目的
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      接口对齐会
    </lark-td>
    <lark-td>
      每周
    </lark-td>
    <lark-td>
      模块上下游负责人
    </lark-td>
    <lark-td>
      确认msg/srv/action变更
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      联调日
    </lark-td>
    <lark-td>
      每两周
    </lark-td>
    <lark-td>
      全团队
    </lark-td>
    <lark-td>
      跨模块集成测试
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      架构评审
    </lark-td>
    <lark-td>
      按需
    </lark-td>
    <lark-td>
      端侧软件负责人+架构组
    </lark-td>
    <lark-td>
      重大设计决策
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      供应商对接会
    </lark-td>
    <lark-td>
      每月
    </lark-td>
    <lark-td>
      语音交互负责人+采购
    </lark-td>
    <lark-td>
      科大讯飞等供应商技术对齐
    </lark-td>
  </lark-tr>
</lark-table>

### 4.3 代码协作规范

- **接口先行**：msg/srv/action 变更必须提前1周通知下游
- **版本管理**：ROS2接口包独立仓库，语义化版本
- **CI门禁**：所有PR必须通过单元测试+代码审查
- **联调环境**：共享一台实体机器人+仿真环境（Isaac Sim / Gazebo）

---

## 五、招聘优先级与时间线

### 5.1 Phase 1 招聘节奏（入职顺序）
```plaintext
Month 1: 端侧软件负责人（P0）
Month 1-2: 中间件工程师（P0）、HAL工程师（P0）
Month 2-3: 运控算法工程师x2（P0）、音频算法工程师（P0）
Month 3-4: 本地ASR工程师（P0）、步态/平衡工程师（P1）
Month 4-5: SLAM工程师（P1）、Agent工程师（P1）
Month 5-6: 交互工程师（P2）、TTS工程师（P1）

```

### 5.2 关键岗位招聘难度评估

<lark-table rows="10" cols="4" header-row="true" column-widths="183,183,183,183">

  <lark-tr>
    <lark-td>
      岗位
    </lark-td>
    <lark-td>
      市场稀缺度
    </lark-td>
    <lark-td>
      建议薪资溢价
    </lark-td>
    <lark-td>
      招聘渠道
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      端侧软件负责人
    </lark-td>
    <lark-td>
      极高
    </lark-td>
    <lark-td>
      +30-50%
    </lark-td>
    <lark-td>
      猎头/行业人脉
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      运控算法工程师（足式）
    </lark-td>
    <lark-td>
      极高
    </lark-td>
    <lark-td>
      +30-40%
    </lark-td>
    <lark-td>
      高校实验室/竞品
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      步态/平衡工程师（WBC/RL）
    </lark-td>
    <lark-td>
      极高
    </lark-td>
    <lark-td>
      +30-40%
    </lark-td>
    <lark-td>
      论文作者/竞赛选手
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      Agent工程师（LLM+机器人）
    </lark-td>
    <lark-td>
      高
    </lark-td>
    <lark-td>
      +20-30%
    </lark-td>
    <lark-td>
      大厂AI部门
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      中间件工程师（ROS2专家）
    </lark-td>
    <lark-td>
      中
    </lark-td>
    <lark-td>
      +10-20%
    </lark-td>
    <lark-td>
      自动驾驶/无人机
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      音频算法工程师
    </lark-td>
    <lark-td>
      中
    </lark-td>
    <lark-td>
      +10-20%
    </lark-td>
    <lark-td>
      语音AI公司
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      本地ASR工程师
    </lark-td>
    <lark-td>
      中高
    </lark-td>
    <lark-td>
      +15-25%
    </lark-td>
    <lark-td>
      科大讯飞/百度/阿里
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      SLAM工程师
    </lark-td>
    <lark-td>
      中
    </lark-td>
    <lark-td>
      +10-20%
    </lark-td>
    <lark-td>
      自动驾驶/无人机
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      HAL工程师
    </lark-td>
    <lark-td>
      中低
    </lark-td>
    <lark-td>
      基准
    </lark-td>
    <lark-td>
      嵌入式大厂
    </lark-td>
  </lark-tr>
</lark-table>

### 5.3 外包/合作策略

<lark-table rows="8" cols="3" header-row="true" column-widths="244,244,244">

  <lark-tr>
    <lark-td>
      模块/能力
    </lark-td>
    <lark-td>
      建议策略
    </lark-td>
    <lark-td>
      说明
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      云端ASR/TTS
    </lark-td>
    <lark-td>
      供应商方案（科大讯飞）
    </lark-td>
    <lark-td>
      自研ROI低，对接即可
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      基础音频前端（AEC/NS）
    </lark-td>
    <lark-td>
      供应商方案 / 开源 webrtc
    </lark-td>
    <lark-td>
      RK3328板载算法或自研
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      视觉SLAM
    </lark-td>
    <lark-td>
      开源+自研优化
    </lark-td>
    <lark-td>
      ORB-SLAM3 / OpenVINS 为基础
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      Lidar SLAM
    </lark-td>
    <lark-td>
      开源+自研优化
    </lark-td>
    <lark-td>
      FAST-LIO / Livox-LOAM 为基础
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      LLM Base Model
    </lark-td>
    <lark-td>
      供应商/开源
    </lark-td>
    <lark-td>
      自研应用层+LoRA适配
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      3D目标检测
    </lark-td>
    <lark-td>
      开源模型+部署优化
    </lark-td>
    <lark-td>
      YOLO-World / RT-DETR
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      手势识别
    </lark-td>
    <lark-td>
      开源+自研优化
    </lark-td>
    <lark-td>
      MediaPipe Hands 为基础
    </lark-td>
  </lark-tr>
</lark-table>

---

## 六、总结

### Phase 1（MVP）总人力

<lark-table rows="4" cols="3" header-row="true" column-widths="244,244,244">

  <lark-tr>
    <lark-td>
      团队
    </lark-td>
    <lark-td>
      人数
    </lark-td>
    <lark-td>
      核心交付
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      端侧软件团队
    </lark-td>
    <lark-td>
      8-10人
    </lark-td>
    <lark-td>
      机器人站立行走、语音对话Demo、基础导航
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      语音交互团队
    </lark-td>
    <lark-td>
      3-4人
    </lark-td>
    <lark-td>
      唤醒+ASR+TTS端到端、基础音频前端
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **合计**
    </lark-td>
    <lark-td>
      **11-14人**
    </lark-td>
    <lark-td>
      可演示的小型人形机器人
    </lark-td>
  </lark-tr>
</lark-table>

### Phase 2（量产）总人力

<lark-table rows="4" cols="3" header-row="true" column-widths="244,244,244">

  <lark-tr>
    <lark-td>
      团队
    </lark-td>
    <lark-td>
      人数
    </lark-td>
    <lark-td>
      核心交付
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      端侧软件团队
    </lark-td>
    <lark-td>
      12-15人
    </lark-td>
    <lark-td>
      稳定行走、复杂任务执行、量产工具链
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      语音交互团队
    </lark-td>
    <lark-td>
      5-6人
    </lark-td>
    <lark-td>
      多模态交互、声纹识别、离线TTS
    </lark-td>
  </lark-tr>
  <lark-tr>
    <lark-td>
      **合计**
    </lark-td>
    <lark-td>
      **17-21人**
    </lark-td>
    <lark-td>
      可量产的小型人形机器人
    </lark-td>
  </lark-tr>
</lark-table>

### 风险提示

1. **运控算法是瓶颈**：足式人形运控人才极度稀缺，建议同时培养内部人才+外部顾问
1. **Agent与运控的gap**：LLM Agent人才多来自互联网，缺乏机器人背景，需要与运控团队紧密协作
1. **语音交互供应商依赖**：科大讯飞等供应商的SDK适配和商务条款需要早期锁定
1. **仿真到真实的gap**：Sim-to-real是人形机器人普遍难题，建议预留20%时间用于实机调试

