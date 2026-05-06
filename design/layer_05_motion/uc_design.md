---

# Upper Body Control 模块设计

## 1. 模块概述与定位

**模块名称**：Upper Body Control（UC）

**定位**：UC 是端侧软件系统中**上肢运动控制插件**，作为 MC（Motion Control）的 Lower Body Controller 对等组件运行。它负责将上层模块（TE/Agent）的末端执行器目标（位姿/力/夹爪动作）转换为上肢关节指令，通过 MC 的插件接口输出。UC 是两种形态（足式/轮式）的**公共组件**，实现传统机械臂控制算法栈。

**核心职责**：
1. **逆运动学（IK）求解**：将末端执行器目标位姿映射到上肢关节角度
2. **轨迹规划**：在操作空间或关节空间中生成平滑、无碰撞的运动轨迹
3. **末端力/阻抗控制**：实现导纳控制和阻抗控制，支持接触任务
4. **夹爪/手控制**：控制夹爪开合、夹持力
5. **自碰检测与避碰**：实时检测上肢与自身/躯干/头部的碰撞风险
6. **基座扰动补偿**：根据 LC 提供的步态相位/升降柱状态补偿基座运动对末端精度的影响
7. **升降柱运动补偿（轮式）**：升降柱高度变化时调整 IK 基座参考，保持末端稳定

**与相邻模块的边界**：

| 边界 | UC 负责 | 对方负责 |
|------|---------|---------|
| UC ↔ MC | 接收 BaseState/LowerBodyStatus/EndEffectorCommand，输出 JointCommand/UpperBodyStatus | 协调层、状态估计、安全校验、EtherCAT 下发 |
| UC ↔ TE（间接） | 通过 MC 接收末端目标 | 任务调度、动作序列生成 |
| UC ↔ Agent（间接） | 通过 MC 接收自主决策的末端指令 | VLA 决策、技能执行 |
| UC ↔ HDS（间接） | 通过 MC 上报 IK 失败、自碰检测、力控异常 | 故障诊断与定级 |

**形态适配说明**：

| 形态 | UC 行为差异 |
|------|------------|
| 足式 | 订阅步态相位做摆臂补偿；基座扰动较大，需预测补偿 |
| 轮式 | 订阅升降柱高度做 IK 基座补偿；基座扰动较小，补偿简单 |

---

## 2. 职责边界

**UC 做的事情**：
- 上肢关节的 IK、轨迹规划、插值
- 末端执行器的位控/力控/混合控制
- 夹爪的开合和力控制
- 上肢与自身/躯干/头部的碰撞检测
- 基座运动扰动的实时补偿

**UC 不做的事情**（红线）：
- **不做下肢控制** — 下肢关节由 LC 插件控制
- **不做状态估计** — 基座姿态和质心状态由 MC 状态估计器提供
- **不直接操作 EtherCAT** — 所有关节指令通过 MC 插件接口输出
- **不做全局路径规划** — 末端目标由上层（TE/Agent）提供
- **不做故障定级** — 只上报原始异常数据，由 HDS 定级
- **不跳过安全校验** — UC 输出指令经过 MC 的 Safety Guardian 后才下发

---

## 3. 状态机设计

### 3.1 UC 内部状态枚举

UC 维护独立的上肢操作状态机，与 MC 的运动模式解耦。MC 运动模式变化通过 `on_motion_mode_changed()` 通知 UC，UC 自主决定内部状态响应。

| 状态 | 值 | 说明 |
|------|-----|------|
| `UC_IDLE` | 0 | 空闲，上肢保持当前位置或阻尼状态 |
| `UC_TRACKING` | 1 | 跟踪末端目标位姿（操作空间轨迹跟踪）|
| `UC_FORCE_CONTROL` | 2 | 末端力/阻抗控制模式 |
| `UC_GRIPPER_ACTION` | 3 | 夹爪动作执行中（开合/夹持）|
| `UC_FAULT` | 4 | 故障（IK 无解/自碰/超限/力控异常）|

### 3.2 状态转换图

```
                              ┌───────────────────────────────────────────────┐
                              │                                               │
                        ┌─────┴──────┐   ee_target     ┌───────────────────┴───┐
                  ┌──►  │   UC_IDLE  │──────────────────►│     UC_TRACKING       │
                  │     │     0      │                   │         1             │
                  │     └────────────┘                   └───────────┬───────────┘
                  │           ▲                                      │
                  │           │         tracking_complete            │
                  │           │    ┌─────────────────────────────────┘
                  │           │    │ force_mode_cmd
                  │           │    ▼
                  │           │  ┌──────────────────┐   tracking   ┌───────────┐
                  │           │  │ UC_FORCE_CONTROL │◄────────────│UC_TRACKING│
                  │           │  │        2         │              │     1     │
                  │           │  └────────┬─────────┘              └───────────┘
                  │           │           │ position_mode_cmd
                  │           │           ▼
                  │           │  ┌──────────────────┐   fault    ┌───────────┐
                  │           │  │   UC_FAULT       │◄───────────│  ALL      │
                  │           │  │       4          │            │  STATES   │
                  │           │  └────────┬─────────┘            └───────────┘
                  │           │           │ reset
                  │           │           ▼
                  │           │  ┌──────────────────┐  gripper_cmd  ┌───────────┐
                  │           │  │ UC_GRIPPER_ACTION│◄──────────────│ UC_TRACKING│
                  │           │  │       3          │               │     1     │
                  │           │  └────────┬─────────┘               └───────────┘
                  │           │           │ gripper_complete
                  │           └───────────┘
                  │
                  └──────────────────────────────────────────────────────────────┘

    MC: MOTION_MODE_IDLE ──► UC_IDLE（强制）
    MC: E-Stop ──► UC_IDLE（强制）
```

### 3.3 状态说明

- **UC_IDLE → UC_TRACKING**：收到 `EndEffectorCommand`（control_type = 位姿控制），UC 规划轨迹并开始跟踪
- **UC_TRACKING → UC_FORCE_CONTROL**：收到模式切换命令（control_type = 力控制），在当前位置切换为力控
- **UC_FORCE_CONTROL → UC_TRACKING**：收到模式切换命令，恢复位姿跟踪
- **UC_TRACKING → UC_GRIPPER_ACTION**：收到 `GripperCommand`，在保持末端位姿的同时执行夹爪动作
- **UC_GRIPPER_ACTION → UC_TRACKING**：夹爪动作完成，恢复末端跟踪
- **任意状态 → UC_FAULT**：IK 无解、自碰检测触发、关节超限、力控异常
- **UC_FAULT → UC_IDLE**：收到 reset 命令，清空错误状态

---

## 4. ROS2 接口定义

UC 作为 MC 内部插件，**没有独立的 ROS2 节点**。所有 ROS2 通信通过 MC 节点代理。上肢相关的消息类型定义在 `mc_msgs` 包中，MC 负责订阅/发布，通过插件接口与 UC 交换数据。

### 4.1 消息定义（msg，存于 mc_msgs）

```
# mc_msgs/msg/EndEffectorTarget.msg
# 末端执行器目标（TE/Agent → MC → UC）

string side                    # "left" / "right"
geometry_msgs/Pose target_pose # 目标位姿（基座坐标系）
geometry_msgs/Wrench target_wrench # 目标力/力矩（力控时）
uint8 control_type             # 0=位姿控制, 1=力控制, 2=混合控制
float64 max_velocity           # 最大末端速度 [m/s]
float64 max_acceleration       # 最大末端加速度 [m/s^2]
duration timeout               # 超时时间
builtin_interfaces/Time stamp
```

```
# mc_msgs/msg/GripperCommand.msg
# 夹爪控制命令

string side                    # "left" / "right"
uint8 command                  # 0=打开, 1=闭合, 2=指定位置
float64 position               # 目标开口宽度 [m]（command=2 时）
float64 max_effort             # 最大夹持力 [N]
duration timeout               # 超时时间
builtin_interfaces/Time stamp
```

```
# mc_msgs/msg/UpperBodyStatus.msg
# 上肢状态（UC → MC → 上层）

# 左臂末端
geometry_msgs/Pose left_end_effector_pose
geometry_msgs/Twist left_end_effector_velocity
geometry_msgs/Wrench left_end_effector_wrench
bool left_ik_valid             # 左臂 IK 是否有效

# 右臂末端
geometry_msgs/Pose right_end_effector_pose
geometry_msgs/Twist right_end_effector_velocity
geometry_msgs/Wrench right_end_effector_wrench
bool right_ik_valid            # 右臂 IK 是否有效

# 夹爪状态
bool left_gripper_closed       # 左夹爪是否闭合
float64 left_gripper_position  # 左夹爪开口宽度 [m]
bool right_gripper_closed      # 右夹爪是否闭合
float64 right_gripper_position # 右夹爪开口宽度 [m]

# 自碰检测
bool self_collision_warning    # 接近碰撞阈值
bool self_collision_critical   # 碰撞临界
string collision_pair          # 碰撞关节对名称

# UC 内部状态
uint8 uc_state                 # 当前 UC 状态
uint16 error_code              # 错误码

builtin_interfaces/Time stamp
```

### 4.2 UC 与 MC 的插件接口（C++）

UC 实现 `mc::UpperBodyController` 纯虚类接口（定义详见 `mc_design.md` 第 4.2 节）。

```cpp
class UpperBodyController {
public:
  virtual bool init(
    const std::string& robot_type,
    const std::vector<std::string>& joint_names,
    const rclcpp::Node::SharedPtr& node
  ) = 0;

  virtual bool update(
    const BaseState& base_state,
    const LowerBodyStatus& lb_status,
    const std::vector<EndEffectorCommand>& ee_commands,
    const JointState& current_joints,
    JointCommand& output,
    UpperBodyStatus& status
  ) = 0;

  virtual void on_motion_mode_changed(uint8_t new_mode, uint8_t old_mode) = 0;
  virtual void reset() = 0;
  virtual void emergency_stop() = 0;
  virtual std::string get_name() const = 0;
  virtual std::string get_version() const = 0;
};
```

### 4.3 接口汇总表

#### Topics（通过 MC 节点代理）

| 名称 | 类型 | 方向 | QoS | 频率 | 说明 |
|------|------|------|-----|------|------|
| `/uc/end_effector_target` | `mc_msgs/msg/EndEffectorTarget` | TE/Agent → MC → UC | Reliable, Depth 10 | 事件驱动 | 末端目标位姿/力 |
| `/uc/gripper_command` | `mc_msgs/msg/GripperCommand` | TE/Agent → MC → UC | Reliable, Depth 10 | 事件驱动 | 夹爪控制命令 |
| `/uc/upper_body_status` | `mc_msgs/msg/UpperBodyStatus` | UC → MC → ALL | Best Effort, Depth 1 | 1kHz | 上肢状态（末端位姿、夹爪、自碰）|

#### 插件接口（C++，共享内存）

| 方向 | 数据 | 说明 |
|------|------|------|
| MC → UC | `BaseState` | 基座姿态、质心状态 |
| MC → UC | `LowerBodyStatus` | 下肢/底盘状态（步态、升降柱高度）|
| MC → UC | `EndEffectorCommand` | 末端目标（经 MC 缓存）|
| MC → UC | `JointState` | 当前上肢关节状态 |
| UC → MC | `JointCommand` | 上肢关节指令（位置/速度/力矩）|
| UC → MC | `UpperBodyStatus` | 上肢状态反馈 |

---

## 5. 内部设计

### 5.1 节点/插件架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     UC Plugin (uc_common.so)                            │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                   UC Control Pipeline (1kHz)                     │   │
│  │                                                                  │   │
│  │  Input: BaseState + LowerBodyStatus + EndEffectorCommand         │   │
│  │        + JointState (from MC)                                    │   │
│  │                                                                  │   │
│  │   ┌─────────────────┐                                           │   │
│  │   │ Base Disturbance │                                           │   │
│  │   │ Compensation     │◄── LowerBodyStatus                        │   │
│  │   │                  │    (gait_phase / lift_column_height)      │   │
│  │   └────────┬────────┘                                           │   │
│  │            │ compensated_target                                 │   │
│  │            ▼                                                    │   │
│  │   ┌─────────────────┐    ┌─────────────────┐                   │   │
│  │   │ Self-Collision   │───►│ IK Solver        │                   │   │
│  │   │ Avoidance        │    │                  │                   │   │
│  │   │                  │    │ · 数值 IK (KDL/  │                   │   │
│  │   │ · FCL 距离检测   │    │   TRAC-IK)       │                   │   │
│  │   │ · 关节极限约束   │    │ · 冗余度优化     │                   │   │
│  │   │ · 避碰速度调整   │    │ · 奇异点处理     │                   │   │
│  │   └─────────────────┘    └────────┬────────┘                   │   │
│  │                                    │ target_joints              │   │
│  │                                    ▼                            │   │
│  │   ┌──────────────────────────────────────────────────────────┐  │   │
│  │   │              Trajectory Planner                           │  │   │
│  │   │                                                           │  │   │
│  │   │  模式分支：                                                │  │   │
│  │   │  UC_TRACKING:    最小加加速度轨迹 (jerk-limited)           │  │   │
│  │   │  UC_FORCE_CTRL:  导纳控制器 + 位置环                       │  │   │
│  │   │                                                           │  │   │
│  │   │  输出: joint_positions, joint_velocities, joint_efforts    │  │   │
│  │   └──────────────────────────────────────────────────────────┘  │   │
│  │                                    │                            │   │
│  │                                    ▼                            │   │
│  │   ┌─────────────────┐    ┌─────────────────┐                   │   │
│  │   │ Gripper Ctrl    │◄───│ 状态机控制器     │                   │   │
│  │   │                 │    │                 │                   │   │
│  │   │ · 位置控制      │    │ · 状态转换逻辑   │                   │   │
│  │   │ · 力限制        │    │ · 错误处理       │                   │   │
│  │   └────────┬────────┘    └─────────────────┘                   │   │
│  │            │                                                    │   │
│  │            ▼                                                    │   │
│  │   ┌──────────────────────────────────────────────────────────┐  │   │
│  │   │ Output: JointCommand (上肢关节) + UpperBodyStatus         │  │   │
│  │   └──────────────────────────────────────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                   ROS2 Callback Thread (MC 代理)                 │   │
│  │                                                                  │   │
│  │  · 订阅 /uc/end_effector_target → 缓存到无锁队列                 │   │
│  │  · 订阅 /uc/gripper_command → 缓存到无锁队列                     │   │
│  │  · 发布 /uc/upper_body_status ← 从无锁队列读取                   │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.2 关键组件设计

#### 5.2.1 IK Solver（逆运动学求解器）

- **求解器选择**：TRAC-IK（基于 KDL，支持速度和收敛优化）或自研数值 IK
- **求解频率**：每控制周期（1kHz）求解一次，或按轨迹点插值频率求解
- **冗余度利用**：7DOF 手臂利用冗余自由度优化关节角度（如最小关节速度、远离奇异点）
- **奇异点处理**：
  - 接近奇异点时降低末端速度
  - 使用阻尼最小二乘法（Damped Least Squares）稳定求解
  - 严重奇异时返回 `ik_valid = false`，触发 UC_FAULT
- **基座补偿**：
  - 足式：根据 `BaseState` 实时调整 IK 参考坐标系
  - 轮式：根据 `LowerBodyStatus.lift_column_height` 调整基座高度

#### 5.2.2 Trajectory Planner（轨迹规划器）

**操作空间轨迹规划（默认）**：
- 输入：当前末端位姿 → 目标末端位姿
- 输出：末端位姿序列（位置 + 姿态）
- 算法：最小加加速度轨迹（jerk-limited），各轴独立规划
- 约束：最大线速度、最大角速度、最大加速度
- 时间参数化：根据路径长度和速度约束计算执行时间

**关节空间轨迹规划（备选）**：
- 当操作空间规划频繁遇到奇异点时，切换到关节空间
- 对每关节独立做 jerk-limited 规划
- 需要在关节空间检查自碰

**插值与执行**：
- 轨迹生成后存储为样条曲线（三次或五次）
- 每周期采样目标位姿 → IK 求解 → 关节指令
- 支持在线 replan（目标更新时平滑过渡）

#### 5.2.3 Force Controller（力控制器）

**导纳控制（Admittance Control，默认）**：
```
F_external → [导纳模型] → Δx → 位置指令 → IK → 关节指令

导纳模型：M·Δẍ + D·Δẋ + K·Δx = F_external
```
- M：虚拟质量
- D：虚拟阻尼
- K：虚拟刚度
- 参数可调，适应不同任务（如插入、打磨、抓取）

**阻抗控制（Impedance Control，备选）**：
- 位置偏差 → 力指令
- 适用于需要精确力控制的场景

**力信号来源**：
- 末端六维力传感器（如有）
- 关节力矩估计（通过电机电流）
- 夹爪力传感器（如有）

#### 5.2.4 Self-Collision Avoidance（自碰检测与避碰）

**碰撞检测**：
- 使用 FCL（Flexible Collision Library）进行实时距离计算
- 碰撞模型：使用 URDF 中的碰撞几何体（简化网格/球体/胶囊体）
- 关键碰撞对：
  - 左臂 ↔ 右臂
  - 手臂 ↔ 躯干
  - 手臂 ↔ 头部
  - 手臂 ↔ 下肢（足式行走时）

**碰撞响应**：
- **Warning 距离**（> 5cm）：在轨迹规划中增加排斥势场，轻微调整路径
- **Critical 距离**（< 2cm）：立即减速/停止，上报 self_collision_critical
- **碰撞发生**：触发 E-Stop 或进入 UC_FAULT

**性能优化**：
- 使用包围盒层次结构（BVH）加速碰撞检测
- 只检测当前姿态下可能接近的碰撞对（预计算关节可达空间）
- 目标延迟 < 1ms/周期

#### 5.2.5 Base Disturbance Compensation（基座扰动补偿）

**足式形态**：
- 订阅 `LowerBodyStatus.gait_phase` 和 `gait_progress`
- 根据步态周期预测基座姿态变化（预先学习的扰动模型或实时补偿）
- 在末端目标中减去预测的基座扰动，保持末端在世界坐标系中稳定
- 实时反馈：根据 `BaseState` 的实际姿态做闭环补偿

**轮式形态**：
- 订阅 `LowerBodyStatus.lift_column_height`
- 升降柱运动时，调整 IK 基座参考高度
- 升降柱速度较快时，增加末端阻尼以保持稳定性

### 5.3 关键设计决策

1. **单臂独立控制**：左右臂独立做 IK 和轨迹规划，互不阻塞。但自碰检测同时考虑双臂
2. **轨迹缓存**：当前执行轨迹缓存 1s，支持平滑 replan（目标更新时用 S 曲线过渡）
3. **力控切换安全**：从位控切换到力控时，在当前位置初始化导纳模型，避免力跳变
4. **IK 失败降级**：IK 无解时，不立即停止，而是尝试：
   -  Relax 冗余优化目标，放宽求解精度
   -  仍无解时，保持上一周期关节角度，触发 warning
   -  连续 10 周期无解，进入 UC_FAULT

### 5.4 关键流程

#### 5.4.1 系统启动流程

```
EM 启动 MC 进程
  → MC Plugin Manager 加载 uc_common.so
  → UC init() 被调用
      → 加载 URDF，构建运动学树
      → 初始化 IK Solver（TRAC-IK）
      → 初始化 FCL 碰撞模型
      → 加载关节限位参数
      → 初始化导纳控制器参数
      → 初始化轨迹规划器
  → UC 返回 init_success
  → MC 进入 IDLE，等待运动指令
```

#### 5.4.2 末端跟踪流程（UC_TRACKING）

```
每 1ms（MC 协调层调用 UC update()）：
  1. 读取当前关节状态（JointState）
  2. 读取末端目标（EndEffectorCommand，MC 缓存）
  3. 基座扰动补偿：
      · 足式：根据 gait_phase 预测基座扰动，补偿到目标位姿
      · 轮式：根据 lift_column_height 调整 IK 基座高度
  4. 自碰检测：计算当前姿态下各碰撞对距离
  5. 轨迹规划：
      · 若目标更新：重新规划 jerk-limited 轨迹
      · 若目标未变：继续执行当前轨迹，采样下一目标点
  6. IK 求解：目标位姿 → 关节角度
      · 成功：得到 target_joints
      · 失败：尝试放宽约束重解 → 仍失败则保持上一周期角度
  7. 关节指令生成：
      · position = target_joints
      · velocity = (target - current) / dt（前馈）
      · effort = 0（由 MC 底层伺服环处理）
  8. 输出 JointCommand + UpperBodyStatus
```

#### 5.4.3 力控流程（UC_FORCE_CONTROL）

```
每 1ms：
  1. 读取末端力/力矩（力传感器或关节力矩估计）
  2. 导纳模型计算：F_external → Δx（期望位置偏移）
  3. 新目标位姿 = 当前位姿 + Δx
  4. IK 求解新目标位姿 → 关节角度
  5. 输出 JointCommand（位置控制模式，导纳输出作为位置目标）
  6. 力控异常检测：
      · 力超限 → 降低导纳刚度，减缓响应
      · 力持续超限 → 进入 UC_FAULT
```

#### 5.4.4 夹爪控制流程（UC_GRIPPER_ACTION）

```
收到 GripperCommand（通过 MC 缓存）：
  → 解析 command（打开/闭合/指定位置）
  → 若 command = 打开：
      · 输出 gripper 关节目标位置 = 最大开度
      · effort_limit = 低力矩（防止夹到物体时过冲）
  → 若 command = 闭合：
      · 输出 gripper 关节目标位置 = 0
      · effort_limit = max_effort（夹持力限制）
      · 检测电流/力判断夹持完成
  → 若 command = 指定位置：
      · 输出 gripper 关节目标位置 = position
      · effort_limit = max_effort
  → 夹爪动作完成后，返回 UC_TRACKING（或保持当前状态）
```

#### 5.4.5 插件异常处理流程

```
UC update() 检测到异常：
  → IK 无解：
      · 第 1-3 次：放宽约束重试，上报 warning
      · 第 4-10 次：保持上一周期角度，持续 warning
      · > 10 次：返回 false，MC 收到 ERR_UC_UPDATE_FAILED
  → 自碰 Critical：
      · 立即停止运动（输出当前位置）
      · 返回 false，MC 进入安全模式
  → 力控异常：
      · 力超限：降低导纳刚度
      · 持续超限：返回 false，MC 进入安全模式
  → MC 处理：
      · 连续 N 周期失败后降级
      · 上肢进入阻尼模式，下肢继续运行
```

---

## 6. 与其他模块的交互

### 6.1 交互矩阵

| 模块 | 方向 | 接口 | 说明 |
|------|------|------|------|
| MC | MC → UC | 插件接口 `update()` | 基座状态、下肢状态、末端目标、关节状态 |
| MC | UC → MC | 插件接口返回值 | 关节指令、上肢状态、错误码 |
| TE（间接）| TE → MC | `/uc/end_effector_target` (Topic) | 末端目标（MC 订阅后转发给 UC）|
| TE（间接）| TE → MC | `/uc/gripper_command` (Topic) | 夹爪命令（MC 订阅后转发给 UC）|
| Agent（间接）| Agent → MC | `/uc/end_effector_target` (Topic) | 自主决策的末端目标 |
| HDS（间接）| UC → MC → HDS | `/hds/health_report` (Topic) | IK 失败、自碰、力控异常 |
| DR（间接）| UC → MC → DR | `/mc/whole_body_state` (Topic) | 上肢末端位姿记录 |

### 6.2 MC 运动模式 → UC 行为映射

| MC 运动模式 | UC 行为 |
|------------|---------|
| `MC_MODE_IDLE` | UC 进入 IDLE，上肢保持位置或阻尼 |
| `MC_MODE_STAND` | UC 进入 TRACKING（如有末端目标）或 IDLE |
| `MC_MODE_READY` | UC 进入 IDLE，等待指令 |
| `MC_MODE_SQUAT` | UC 调整手臂姿态以避免触碰地面（足式）|
| `MC_MODE_SIT` | UC 调整手臂姿态到舒适位置（足式）|
| `MC_MODE_MOTION` | UC 配合 MP 执行预录动作（关节空间跟踪）|
| `MC_MODE_WALKING` | UC 进入 TRACKING + 摆臂补偿（如需要）|
| `MC_MODE_OPERATING` | UC 专注末端跟踪/力控，下肢由 LC 保持稳定 |
| `MC_MODE_ZERO_TORQUE` | UC 输出 effort = 0 |
| `MC_MODE_DAMPING` | UC 输出高阻尼指令 |

---

## 7. 关键参数与配置

```yaml
# uc_common/config/uc_params.yaml

upper_body_control:
  ros__parameters:
    # ========== URDF 配置 ==========
    urdf_path: "$(find uc_common)/urdf/upper_body.urdf"

    # ========== 关节列表 ==========
    left_arm_joints:
      - "left_shoulder_pitch"
      - "left_shoulder_roll"
      - "left_shoulder_yaw"
      - "left_elbow_pitch"
      - "left_wrist_yaw"
      - "left_wrist_roll"
      - "left_wrist_pitch"
    right_arm_joints:
      - "right_shoulder_pitch"
      - "right_shoulder_roll"
      - "right_shoulder_yaw"
      - "right_elbow_pitch"
      - "right_wrist_yaw"
      - "right_wrist_roll"
      - "right_wrist_pitch"
    gripper_joints:
      - "left_gripper"
      - "right_gripper"
    torso_joints:
      - "waist_yaw"
    neck_joints:
      - "neck_yaw"
      - "neck_pitch"

    # ========== IK 参数 ==========
    ik:
      solver: "trac_ik"           # trac_ik / kdl / custom
      timeout_ms: 5.0             # IK 求解超时 [ms]
      epsilon: 0.001              # 求解精度 [m]
      solve_type: "Speed"         # Speed / Distance / Manip1 / Manip2
      redundancy_optimization: true

    # ========== 轨迹规划参数 ==========
    trajectory:
      planning_space: "operational"   # operational / joint
      max_linear_velocity: 0.5        # 末端最大线速度 [m/s]
      max_linear_acceleration: 2.0    # 末端最大线加速度 [m/s^2]
      max_angular_velocity: 1.0       # 末端最大角速度 [rad/s]
      max_angular_acceleration: 4.0   # 末端最大角加速度 [rad/s^2]
      jerk_limit: 10.0                # 加加速度限制 [m/s^3]
      replan_blend_time: 0.2          # 重规划混合时间 [s]

    # ========== 导纳控制参数 ==========
    admittance:
      virtual_mass: [5.0, 5.0, 5.0, 0.5, 0.5, 0.5]       # M [kg, kg·m^2]
      virtual_damping: [50.0, 50.0, 50.0, 5.0, 5.0, 5.0] # D [N·s/m, N·m·s/rad]
      virtual_stiffness: [100.0, 100.0, 100.0, 10.0, 10.0, 10.0]  # K [N/m, N·m/rad]
      force_deadband: 2.0             # 力死区 [N]
      torque_deadband: 0.2            # 力矩死区 [N·m]

    # ========== 自碰检测参数 ==========
    self_collision:
      enable: true
      library: "fcl"                  # fcl / custom
      warning_distance: 0.05          # 警告距离 [m]
      critical_distance: 0.02         # 临界距离 [m]
      check_pairs:                    # 碰撞检测对
        - ["left_arm", "right_arm"]
        - ["left_arm", "torso"]
        - ["right_arm", "torso"]
        - ["left_arm", "head"]
        - ["right_arm", "head"]

    # ========== 基座扰动补偿参数 ==========
    base_compensation:
      enable: true
      # 足式形态参数
      bipedal:
        gait_compensation_gain: 0.8   # 步态补偿增益
        base_filter_cutoff: 10.0      # 基座姿态低通滤波截止频率 [Hz]
      # 轮式形态参数
      wheeled:
        lift_column_compensation: true
        stability_damping_gain: 2.0   # 升降柱运动时的阻尼增益

    # ========== 夹爪参数 ==========
    gripper:
      max_opening: 0.08               # 最大开口宽度 [m]
      close_effort_limit: 20.0        # 闭合最大力 [N]
      open_effort_limit: 5.0          # 打开最大力 [N]
      close_timeout: 3.0              # 闭合超时 [s]

    # ========== 安全参数 ==========
    safety:
      ik_failure_threshold: 10        # IK 连续失败阈值 [周期]
      force_overload_threshold: 50.0  # 力超载阈值 [N]
      torque_overload_threshold: 5.0  # 力矩超载阈值 [N·m]
      joint_limit_margin: 0.05        # 关节限位安全裕度 [rad]
```

---

## 8. 错误码定义

| 错误码 | 常量名 | 说明 | 严重级别 |
|--------|--------|------|---------|
| 0 | `UC_OK` | 成功 | — |
| 5001 | `UC_ERR_IK_NO_SOLUTION` | IK 无解（目标位姿不可达）| HIGH |
| 5002 | `UC_ERR_IK_SINGULARITY` | 接近奇异点，求解不稳定 | MEDIUM |
| 5003 | `UC_ERR_IK_TIMEOUT` | IK 求解超时 | MEDIUM |
| 5004 | `UC_ERR_TRAJ_PLAN_FAIL` | 轨迹规划失败（约束冲突）| HIGH |
| 5005 | `UC_ERR_TRAJ_LIMIT_VIOLATION` | 轨迹超出关节限位 | HIGH |
| 5006 | `UC_ERR_SELF_COLLISION_WARN` | 自碰警告（距离 < warning_threshold）| LOW |
| 5007 | `UC_ERR_SELF_COLLISION_CRITICAL` | 自碰临界（距离 < critical_threshold）| CRITICAL |
| 5008 | `UC_ERR_FORCE_OVERLOAD` | 末端力超载 | HIGH |
| 5009 | `UC_ERR_TORQUE_OVERLOAD` | 末端力矩超载 | HIGH |
| 5010 | `UC_ERR_GRIPPER_TIMEOUT` | 夹爪动作超时 | MEDIUM |
| 5011 | `UC_ERR_GRIPPER_FORCE_FAIL` | 夹爪力控制失败 | MEDIUM |
| 5012 | `UC_ERR_INVALID_TARGET` | 无效末端目标（空/NaN）| LOW |
| 5013 | `UC_ERR_JOINT_LIMIT_VIOLATION` | 关节指令超出限位 | HIGH |

---

## 9. 安全约束

### 9.1 关节限位保护

- UC 内部维护关节软限位（硬限位减去 `joint_limit_margin`）
- IK 求解时以软限位为约束
- 轨迹规划输出超出软限位时，裁剪到安全范围并上报 warning
- 超出硬限位时，返回 false，MC 进入安全模式

### 9.2 自碰安全层

- **Warning**：调整轨迹避碰，继续执行，上报 `UC_ERR_SELF_COLLISION_WARN`
- **Critical**：立即停止运动，返回 false，MC 触发 `ERR_COMMAND_AGGREGATION` 或进入 IDLE
- 自碰检测在 UC 内部完成，MC 只负责接收 UC 上报的 `self_collision_critical` 标志

### 9.3 力控安全层

- 导纳控制的虚拟质量/阻尼/刚度参数经过严格验证，避免共振
- 力信号经过低通滤波，避免噪声引起的抖动
- 力超载时：
  - 第 1 次：降低导纳刚度，减缓响应
  - 连续 3 次：进入 UC_FAULT，MC 进入阻尼模式

### 9.4 基座扰动补偿安全

- 补偿量限制在物理可行范围内（如末端速度不超过 `max_linear_velocity`）
- 基座姿态数据丢失时（ stale > 10ms ），停止补偿，使用上一周期数据
- 补偿导致的末端偏移超过阈值时，上报 warning

---

## 10. 包结构

```
uc_common/
├── include/uc_common/
│   ├── uc_plugin.hpp                 # UpperBodyController 实现类
│   ├── ik_solver.hpp                 # IK 求解器封装
│   ├── trajectory_planner.hpp        # 轨迹规划器
│   ├── force_controller.hpp          # 导纳/阻抗控制器
│   ├── gripper_controller.hpp        # 夹爪控制器
│   ├── self_collision_checker.hpp    # 自碰检测器
│   ├── base_compensator.hpp          # 基座扰动补偿器
│   └── utils.hpp                     # 工具函数（位姿插值、坐标变换等）
├── src/
│   ├── uc_plugin.cpp
│   ├── ik_solver.cpp
│   ├── trajectory_planner.cpp
│   ├── force_controller.cpp
│   ├── gripper_controller.cpp
│   ├── self_collision_checker.cpp
│   ├── base_compensator.cpp
│   └── utils.cpp
├── config/
│   └── uc_params.yaml                # UC 参数配置
├── test/
│   ├── test_ik_solver.cpp            # IK 求解器单元测试
│   ├── test_trajectory_planner.cpp   # 轨迹规划器单元测试
│   ├── test_force_controller.cpp     # 力控制器单元测试
│   ├── test_self_collision.cpp       # 自碰检测单元测试
│   ├── test_base_compensator.cpp     # 基座补偿单元测试
│   └── test_integration.cpp          # 集成测试（含 mock MC）
├── urdf/
│   └── upper_body.urdf               # 上肢 URDF（用于运动学和碰撞检测）
├── CMakeLists.txt
└── package.xml
```

> **编译输出**：`libuc_common_plugin.so`，安装到系统 lib 目录，MC 通过 `library_path` 参数加载。

---

## 11. 关键性能指标（KPI）

| 指标 | 目标值 |
|------|--------|
| UC update() 执行时间 | < 500us（含 IK + 轨迹采样 + 自碰检测）|
| IK 求解时间 | < 2ms（单次求解，TRAC-IK）|
| 轨迹规划时间 | < 5ms（replan 时，非每周期）|
| 自碰检测时间 | < 1ms（FCL，5 对碰撞体）|
| 导纳控制延迟 | < 1ms（力信号 → 位置偏移 → 关节指令）|
| 末端跟踪精度 | 位置误差 < 5mm，姿态误差 < 2° |
| 力控精度 | 力误差 < 2N（稳态）|
| 夹爪控制精度 | 位置误差 < 1mm |
| 插件内存占用 | < 100MB（含 URDF、碰撞模型）|

---

## 附录 A：与 MC 的插件接口详细说明

### A.1 `init()` 调用时机

MC 启动时，Plugin Manager 加载 `.so` 后，在 ROS2 回调线程中调用 `init()`。此时：
- ROS2 节点已初始化，UC 可以通过 `node` 参数访问参数服务器
- EtherCAT 尚未就绪，UC 不应发送关节指令
- `init()` 成功返回后，MC 进入待命状态

### A.2 `update()` 调用约束

- 调用频率：1kHz（由 MC 实时线程驱动）
- 执行时间：必须在 500us 内完成，否则影响 MC 控制循环
- 线程安全：`update()` 在实时线程中执行，不得：
  - 调用 ROS2 Service（阻塞）
  - 申请动态内存（malloc/new）
  - 持有锁超过 10us
  - 执行文件 I/O
- 异常处理：`update()` 返回 false 表示本周期失败，MC 累计 N 周期后降级

### A.3 `emergency_stop()` 调用约束

- 调用时机：E-Stop 触发时，MC 在 ROS2 回调线程中调用
- 执行时间：必须 < 1ms
- 职责：清理 UC 内部状态（停止轨迹、重置力控模型、关闭夹爪力控制）
- 禁止：执行阻塞操作、发送 ROS2 消息、访问硬件

## 附录 B：上肢消息类型在 mc_msgs 中的位置

UC 没有独立的 `_msgs` 包。以下消息类型定义在 `mc_msgs` 中：

| 消息类型 | 文件路径 | 用途 |
|---------|---------|------|
| `EndEffectorTarget` | `mc_msgs/msg/EndEffectorTarget.msg` | 末端目标（TE/Agent → MC）|
| `GripperCommand` | `mc_msgs/msg/GripperCommand.msg` | 夹爪命令（TE/Agent → MC）|
| `UpperBodyStatus` | `mc_msgs/msg/UpperBodyStatus.msg` | 上肢状态（MC → 上层）|

MC 负责订阅 `/uc/end_effector_target` 和 `/uc/gripper_command`，缓存到无锁队列，在实时线程中通过 `EndEffectorCommand` 结构传递给 UC。
