# 编程规范（Google Style + ROS2 扩展）

> 基于 [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)、[Google Python Style Guide](https://google.github.io/styleguide/pyguide.html) 制定，结合 ROS2 项目实际进行扩展和裁剪。

---

## 1. 通用原则

### 1.1 代码是写给人看的

- 优先选择可读性，而非巧妙或过度优化
- 一个函数只做一件事，一个类只有一个职责
- 使用自解释的命名，减少注释依赖
- 注释解释 "为什么"，而非 "做什么"

### 1.2 保持一致性

- 遵循本规范的所有条款
- 当规范未覆盖时，参考同一文件/模块中的既有代码风格
- 不要混合风格：新增代码必须与周边代码风格一致

### 1.3 最小权限原则

- 变量、函数、类使用最小可见性
- 默认 `private`，需要时提升为 `protected`/`public`
- 不暴露内部实现细节

---

## 2. C++ 编码规范

### 2.1 文件与编码

| 项目 | 规范 |
|------|------|
| 文件编码 | UTF-8 |
| 换行符 | LF (`\n`) |
| 缩进 | 2 个空格（禁止 Tab） |
| 行宽 | 100 字符（最多 120） |
| 文件命名 | `snake_case.cpp` / `snake_case.hpp` |
| 头文件保护 | `#ifndef PROJECT_PATH_FILE_HPP_` |

**头文件保护示例：**

```cpp
#ifndef STRIDING_MC_MOTION_CONTROL_NODE_HPP_
#define STRIDING_MC_MOTION_CONTROL_NODE_HPP_

// ... 内容 ...

#endif  // STRIDING_MC_MOTION_CONTROL_NODE_HPP_
```

**现代 C++ 推荐 `#pragma once`（二选一，文件内保持一致）：**

```cpp
#pragma once
```

### 2.2 命名规范

| 类型 | 规则 | 示例 |
|------|------|------|
| 类型名（class/struct/enum/typedef） | PascalCase | `MotionController`, `RobotState` |
| 变量名（成员/局部/参数） | snake_case | `joint_position`, `timeout_sec` |
| 类成员变量 | 尾部下划线 | `joint_states_`, `timer_` |
| 常量（含 constexpr/const） | k 前缀 + PascalCase | `kMaxJointCount`, `kDefaultTimeoutMs` |
| 宏 / 枚举值 | 全大写 + 下划线 | `MAX_RETRY_COUNT`, `MODE_NORMAL` |
| 函数名（自由函数） | PascalCase | `ComputeIK()`, `ValidateState()` |
| 函数名（类方法，ROS2 节点公共接口） | snake_case | `on_joint_state_received()` |
| 函数名（accessor/mutator） | snake_case | `robot_state()`, `set_robot_state()` |
| 文件名 | snake_case | `motion_control_node.cpp` |
| 命名空间 | 全小写 | `striding::mc` |
| 模板参数 | 单个大写字母或 PascalCase | `T`, `InputIt` |

**命名示例：**

```cpp
namespace striding::mc {

class MotionController {
 public:
  static constexpr int kMaxJointCount = 32;

  explicit MotionController(const rclcpp::NodeOptions& options);

  RobotState GetCurrentState() const;
  void SetTargetPose(const Pose& target_pose);

 private:
  void OnJointStateReceived(const sensor_msgs::msg::JointState::SharedPtr msg);
  bool ValidateJointLimits(const std::vector<double>& positions);

  rclcpp::Node::SharedPtr node_;
  std::vector<double> joint_positions_;
  std::mutex state_mutex_;
};

}  // namespace striding::mc
```

### 2.3 格式规范

#### 大括号

- 函数/类/控制语句：左大括号不换行（K&R 风格）

```cpp
// 正确
void ProcessData(const Data& data) {
  if (data.empty()) {
    return;
  }
  for (size_t i = 0; i < data.size(); ++i) {
    // ...
  }
}

// 错误
void ProcessData(const Data& data)
{
  if (data.empty())
  {
    return;
  }
}
```

- 单行 `if` 也必须有大括号

```cpp
// 正确
if (condition) {
  return;
}

// 错误
if (condition) return;
```

#### 空格

```cpp
// 正确：操作符两侧有空格
int result = a + b * c;
bool flag = (x == y) && (z != 0);

// 正确：逗号后空格，分号后空格（for 循环）
for (int i = 0; i < count; ++i) {
  DoSomething(a, b, c);
}

// 正确：指针/引用靠近类型
int* ptr;
int& ref;
const std::string& name;

// 错误
int * ptr;
int &ref;
```

#### 指针与引用

```cpp
// 正确：声明每个变量单独一行，* 和 & 靠近类型
int* p1;
int* p2;

// 错误
int* p1, p2;  // p2 是 int，不是 int*
```

### 2.4 头文件规范

#### Include 顺序

```cpp
// 1. 对应的 .hpp（如果是 .cpp 文件）
#include "mc/motion_control_node.hpp"

// 2. C 系统头文件
#include <unistd.h>

// 3. C++ 标准库头文件
#include <memory>
#include <vector>

// 3.5. 第三方库头文件
#include "rclcpp/rclcpp.hpp"

// 4. 本项目头文件
#include "mc/joint_controller.hpp"
#include "mc/trajectory_planner.hpp"
```

- 每个组之间空一行
- 组内按字母顺序排列
- 使用 `""` 包含项目头文件，`<>` 包含系统/第三方头文件
- **禁止**使用 `.h` 后缀包含 C++ 标准库头文件

#### 前向声明优先

```cpp
// 优先前向声明，减少编译依赖
namespace rclcpp {
class Node;
class Timer;
}  // namespace rclcpp

// 避免不必要的完整包含
// #include <rclcpp/rclcpp.hpp>  // 仅在 .cpp 中包含
```

### 2.5 类设计规范

#### 构造函数

```cpp
class TrajectoryPlanner {
 public:
  // 单参数构造函数必须 explicit
  explicit TrajectoryPlanner(const PlannerConfig& config);

  // 禁用拷贝（ROS2 节点典型模式）
  TrajectoryPlanner(const TrajectoryPlanner&) = delete;
  TrajectoryPlanner& operator=(const TrajectoryPlanner&) = delete;

  // 允许移动
  TrajectoryPlanner(TrajectoryPlanner&&) = default;
  TrajectoryPlanner& operator=(TrajectoryPlanner&&) = default;

  // 虚析构函数（如果类有虚函数或可能被继承）
  virtual ~TrajectoryPlanner() = default;
};
```

#### 成员初始化

```cpp
class JointController {
 public:
  JointController(int joint_id, const std::string& name)
    : joint_id_(joint_id),
      joint_name_(name),
      position_(0.0),
      velocity_(0.0) {}

 private:
  const int joint_id_;
  const std::string joint_name_;
  double position_;
  double velocity_;
};
```

#### 访问控制

```cpp
class ExampleNode : public rclcpp::Node {
 public:   // 公共接口
  explicit ExampleNode(const rclcpp::NodeOptions& options);
  bool Initialize();

 private:  // 内部实现
  void SetupCallbacks();
  void OnTimer();

  // 成员变量按功能分组
  // Callback groups
  rclcpp::CallbackGroup::SharedPtr service_cbg_;

  // Publishers & Subscribers
  rclcpp::Publisher<RobotStateMsg>::SharedPtr state_pub_;
  rclcpp::Subscription<JointStateMsg>::SharedPtr joint_sub_;

  // Timers
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;

  // State
  RobotState current_state_;
  std::mutex state_mutex_;
};
```

### 2.6 函数规范

#### 函数长度

- 目标：函数体 ≤ 50 行
- 上限：函数体 ≤ 100 行（超过必须拆分）
- 参数：≤ 6 个（超过用 struct 封装）

#### 参数传递

```cpp
// 值传递：小对象（≤ 16 字节），无修改
void SetId(int id);

// const 引用：只读的大对象
void SetConfig(const PlannerConfig& config);

// 非 const 引用：需要修改的参数（避免使用，优先返回值）
void ComputeResult(const Input& in, Output& out);  // 避免

// 指针：可为空的可选参数
void SetCallback(std::function<void()> callback);  // 可用 nullptr 清空

// 右值引用：移动语义
void SetData(std::vector<double>&& data);

// 返回值优先（C++17 NRVO/拷贝消除）
Trajectory ComputeTrajectory(const Pose& start, const Pose& goal);
```

#### 返回值

```cpp
// 使用 std::optional 表示可能失败但无错误信息的场景
std::optional<Pose> GetLastKnownPose();

// 使用 std::expected（C++23）或返回 bool + 输出参数
// 本项目使用：返回 bool + 错误信息 + 输出参数
bool ComputeIK(const Pose& target, std::vector<double>& joint_positions, std::string& error);

// ROS2 Service 回调标准格式
void HandleService(
    const std::shared_ptr<RequestType::Request> request,
    std::shared_ptr<RequestType::Response> response) {
  try {
    // 业务逻辑
    response->success = true;
    response->message = "ok";
  } catch (const std::exception& e) {
    response->success = false;
    response->message = e.what();
    RCLCPP_ERROR(this->get_logger(), "Service failed: %s", e.what());
  }
}
```

### 2.7 现代 C++ 特性

#### 强制使用

```cpp
// 1. 智能指针（禁止裸指针管理所有权）
std::unique_ptr<Resource> resource = std::make_unique<Resource>();
std::shared_ptr<Node> node = std::make_shared<Node>(options);

// 2. auto（提高可读性，但避免过度使用）
auto msg = std::make_shared<RobotStateMsg>();
auto it = vec.begin();  // 迭代器类型复杂，用 auto
for (const auto& joint : joint_states_) {  // 范围 for
  // ...
}

// 3. 列表初始化
std::vector<double> positions{0.0, 0.0, 0.0};

// 4. constexpr
static constexpr double kPi = 3.14159265358979323846;

// 5. nullptr
int* p = nullptr;  // 不用 NULL 或 0

// 6. override / final
class Derived : public Base {
 public:
  void VirtualMethod() override;
};

// 7. default / delete
class NonCopyable {
 public:
  NonCopyable() = default;
  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;
};
```

#### 禁止使用

```cpp
// 1. 禁止使用裸 new/delete（除了 make_unique/make_shared 内部）
// 错误
Foo* foo = new Foo();
delete foo;

// 2. 禁止使用异常作为正常控制流（异常仅用于异常情况）
// 错误
bool CheckValid() {
  try {
    Validate();
    return true;
  } catch (...) {
    return false;
  }
}

// 3. 禁止使用 dynamic_cast（设计问题，用虚函数替代）
// 4. 禁止使用 RTTI（typeid）
// 5. 禁止使用 C 风格类型转换
// 错误
int x = (int)double_value;
// 正确
int x = static_cast<int>(double_value);

// 6. 禁止使用宏定义常量/函数（用 constexpr/inline 替代）
// 7. 禁止使用 using namespace std;（using 声明可以）
```

### 2.8 线程安全

```cpp
class ThreadSafeBuffer {
 public:
  void Push(const Data& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.push_back(data);
  }

  std::optional<Data> Pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (buffer_.empty()) {
      return std::nullopt;
    }
    Data data = buffer_.front();
    buffer_.pop_front();
    return data;
  }

 private:
  std::deque<Data> buffer_;
  mutable std::mutex mutex_;  // mutable 允许 const 方法加锁
};
```

- 优先使用 `std::lock_guard` / `std::unique_lock`
- 避免裸 `mutex.lock()` / `mutex.unlock()`
- 避免在持有锁时调用外部代码（回调、虚拟函数）

### 2.9 ROS2 节点模板

```cpp
#pragma once

#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

namespace striding::example {

class ExampleNode : public rclcpp::Node {
 public:
  explicit ExampleNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());
  ~ExampleNode() override = default;

  // 禁用拷贝
  ExampleNode(const ExampleNode&) = delete;
  ExampleNode& operator=(const ExampleNode&) = delete;

 private:
  void DeclareParameters();
  void SetupCallbacks();
  void OnTimer();
  void OnMessageReceived(const std_msgs::msg::String::SharedPtr msg);

  // Parameters
  std::string config_param_;
  int timeout_ms_;

  // Callback groups
  rclcpp::CallbackGroup::SharedPtr default_cbg_;
  rclcpp::CallbackGroup::SharedPtr service_cbg_;

  // Publishers / Subscribers / Services / Clients
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr pub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace striding::example
```

---

## 3. Python 编码规范

### 3.1 基本格式

| 项目 | 规范 |
|------|------|
| 编码 | UTF-8 |
| 缩进 | 4 个空格 |
| 行宽 | 100 字符（最多 120） |
| 换行 | LF |
| 引号 | 单引号 `'string'`（文档字符串用 `"""`） |

### 3.2 命名规范

| 类型 | 规则 | 示例 |
|------|------|------|
| 模块/包 | 全小写 + 下划线 | `motion_utils`, `launch_helper` |
| 类 | PascalCase | `TrajectoryPlanner`, `LaunchGenerator` |
| 函数/方法 | snake_case | `compute_trajectory()`, `setup_node()` |
| 变量 | snake_case | `joint_position`, `timeout_sec` |
| 常量 | 全大写 + 下划线 | `MAX_JOINT_COUNT`, `DEFAULT_TIMEOUT` |
| 私有成员 | 前导下划线 | `_internal_state` |
| 强私有 | 双前导下划线 | `__private_method` |
| 类型变量 | PascalCase + _co/_contra | `T`, `KT`, `VT` |

### 3.3 导入顺序

```python
# 1. 标准库
import os
import sys
from pathlib import Path

# 2. 第三方库
import numpy as np
import rclpy
from rclpy.node import Node

# 3. 本项目模块
from mc_utils.trajectory import interpolate
from .constants import MAX_JOINT_COUNT
```

- 每组之间空一行
- 组内按字母顺序排列
- 优先 `from x import y` 而非 `import x.y`

### 3.4 类型注解

```python
from typing import List, Optional, Dict, Tuple

# 函数签名必须有类型注解
def compute_ik(
    target_pose: Pose,
    initial_guess: Optional[List[float]] = None,
    timeout_sec: float = 1.0,
) -> Tuple[bool, List[float], str]:
    """Compute inverse kinematics.

    Args:
        target_pose: Target end-effector pose.
        initial_guess: Optional initial joint configuration.
        timeout_sec: Maximum time to spend on computation.

    Returns:
        Tuple of (success, joint_positions, error_message).
    """
    ...

# 变量类型注解
positions: List[float] = [0.0] * 7
config: Dict[str, float] = {"kp": 100.0, "kd": 10.0}
```

### 3.5 ROS2 Python 节点模板

```python
#!/usr/bin/env python3
"""Example ROS2 Python node."""

import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class ExampleNode(Node):
    """Example node demonstrating Python ROS2 conventions."""

    def __init__(self) -> None:
        super().__init__('example_node')

        self._declare_parameters()
        self._setup_callbacks()

    def _declare_parameters(self) -> None:
        self.declare_parameter('update_rate_hz', 10.0)
        self._update_rate = (
            self.get_parameter('update_rate_hz').value
        )

    def _setup_callbacks(self) -> None:
        self._timer = self.create_timer(
            1.0 / self._update_rate,
            self._on_timer,
        )
        self._pub = self.create_publisher(
            String, '/example/status', 10,
        )

    def _on_timer(self) -> None:
        msg = String()
        msg.data = 'heartbeat'
        self._pub.publish(msg)


def main(args: Optional[List[str]] = None) -> int:
    rclpy.init(args=args)
    node = ExampleNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()
    return 0


if __name__ == '__main__':
    sys.exit(main())
```

---

## 4. 注释与文档规范

### 4.1 C++ 注释

```cpp
// 行内注释：解释"为什么"
int retries = 3;  // 重试 3 次以满足 99.9% 可用性 SLA

/*
 * 块注释：解释复杂算法或设计决策
 * 使用 KMP 算法实现，避免回溯以提高实时性
 */

// TODO(username): 描述待办事项，日期可选
// TODO(alice): 2026-06-01 优化此处的内存分配策略

// FIXME(username): 描述已知问题
// FIXME(bob): 边界条件下可能死锁

// NOTE: 提醒读者注意特殊逻辑
// NOTE: 此处顺序不可交换，因为 ...
```

### 4.2 C++ 文件头注释

```cpp
// Copyright 2026 Striding Robotics
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// ...
//
// =============================================================================
//
// @file   motion_control_node.cpp
// @brief  运动控制主节点，负责协调上肢/下肢/全身运动
// @author alice@striding.ai
// @date   2026-05-27
//
// @copyright Copyright (c) 2026 Striding Robotics
```

### 4.3 C++ Doxygen 注释

```cpp
/**
 * @brief 执行逆运动学计算
 * @param target_pose 目标末端位姿
 * @param initial_guess 初始关节角猜测（可选）
 * @param[out] joint_positions 计算得到的关节角
 * @return true 成功，false 失败（joint_positions 不变）
 * @throws std::invalid_argument 当 target_pose 包含 NaN 时
 *
 * @note 使用 Levenberg-Marquardt 算法，收敛精度 1e-6
 * @pre joint_positions 大小必须为 7
 * @post joint_positions 所有值在 [-pi, pi] 范围内
 */
bool ComputeIK(
    const Pose& target_pose,
    const std::optional<std::vector<double>>& initial_guess,
    std::vector<double>* joint_positions);
```

### 4.4 Python Docstring（Google Style）

```python
def send_motion_command(
    joint_positions: List[float],
    duration_sec: float,
    velocity_limit: Optional[float] = None,
) -> Tuple[bool, str]:
    """Send a motion command to the joint controllers.

    Args:
        joint_positions: Target joint positions in radians.
        duration_sec: Time to reach target in seconds.
        velocity_limit: Optional maximum joint velocity in rad/s.
            If None, uses the default limit from config.

    Returns:
        A tuple containing:
            - bool: True if command was accepted.
            - str: Status message or error description.

    Raises:
        ValueError: If joint_positions length doesn't match DoF.
        TimeoutError: If command buffer is full for > 100ms.

    Example:
        >>> success, msg = send_motion_command(
        ...     [0.0, -0.5, 0.0, 0.0, 0.0, 0.0, 0.0],
        ...     duration_sec=2.0,
        ... )
        >>> print(msg)
        'Command accepted'
    """
```

---

## 5. 错误处理规范

### 5.1 C++ 错误处理

```cpp
// 1. 使用 std::optional 表示可选值
std::optional<RobotState> GetRobotState();

// 2. 返回 bool + 错误信息（C++17 前推荐）
bool ParseConfig(const std::string& path, Config* out_config, std::string* error);

// 3. 返回结构体（C++17 推荐）
struct Result {
  bool success;
  std::string error_message;
  std::variant<Data, Empty> data;
};

// 4. 异常仅用于真正异常的情况
// 正确：配置解析失败 -> 返回 false
// 正确：内存分配失败 -> 抛出 std::bad_alloc（不可恢复）
// 错误：参数验证失败 -> 不应抛异常

// 5. 构造函数失败处理
class SafeInitClass {
 public:
  static std::optional<SafeInitClass> Create(const Config& config);
  // 或
  static std::unique_ptr<SafeInitClass> Create(const Config& config);

 private:
  explicit SafeInitClass(const Config& config);
};
```

### 5.2 日志规范（ROS2 rclcpp）

```cpp
// 级别选择
RCLCPP_DEBUG(node->get_logger(), "Joint position: %.3f", pos);   // 调试信息
RCLCPP_INFO(node->get_logger(), "Node initialized successfully"); // 正常流程
RCLCPP_WARN(node->get_logger(), "Joint %d near limit: %.1f deg", id, angle); // 警告
RCLCPP_ERROR(node->get_logger(), "Failed to connect to EtherCAT: %s", err);  // 错误
RCLCPP_FATAL(node->get_logger(), "Critical fault, entering E-Stop");         // 致命

// 日志格式：使用 fmt 风格（ROS2 自动支持）
RCLCPP_INFO(node->get_logger(), "Motion completed in %.2f sec", duration);

// 避免在热路径中格式化日志（高频场景）
// 错误
for (const auto& joint : joints_) {
  RCLCPP_DEBUG(node->get_logger(), "Joint %s: pos=%f", joint.name_.c_str(), joint.pos_);
}
// 正确：使用 RCLCPP_DEBUG_THROTTLE 或条件判断
```

---

## 6. 测试规范

### 6.1 C++ 测试（Google Test）

```cpp
// test_motion_controller.cpp
#include <gtest/gtest.h>
#include "mc/motion_controller.hpp"

namespace striding::mc {

class MotionControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rclcpp::init(0, nullptr);
    node_ = std::make_shared<rclcpp::Node>("test_node");
  }

  void TearDown() override {
    node_.reset();
    rclcpp::shutdown();
  }

  std::shared_ptr<rclcpp::Node> node_;
};

TEST_F(MotionControllerTest, InitializeSuccess) {
  MotionController controller(node_);
  EXPECT_TRUE(controller.Initialize());
}

TEST_F(MotionControllerTest, RejectMotionInEStop) {
  MotionController controller(node_);
  controller.Initialize();

  // 模拟 E-Stop 状态
  controller.SetState(RobotState::ACTIVE_E_STOP);

  auto result = controller.SendMotionCommand(/* ... */);
  EXPECT_FALSE(result.success);
  EXPECT_EQ(result.error_code, ErrorCode::ESTOP_ACTIVE);
}

}  // namespace striding::mc
```

### 6.2 Python 测试（pytest）

```python
# test_example_node.py
import pytest
from example.example_node import ExampleNode


class TestExampleNode:
    @pytest.fixture(autouse=True)
    def setup(self):
        self.node = ExampleNode()
        yield
        self.node.destroy_node()

    def test_initial_state(self):
        assert self.node.get_state() == NodeState.IDLE

    def test_parameter_default(self):
        assert self.node.get_parameter('update_rate_hz').value == 10.0
```

### 6.3 测试命名

| 类型 | 命名模式 |
|------|----------|
| 测试文件 | `test_{module}.cpp` / `test_{module}.py` |
| 测试夹具 | `{ClassName}Test` |
| 测试用例 | `MethodName_Scenario_ExpectedResult` |

---

## 7. 代码审查清单

提交代码前自检：

- [ ] 编译无警告（`-Wall -Wextra -Werror`）
- [ ] 所有测试通过
- [ ] 命名符合本规范
- [ ] 无硬编码魔法数字（用 `constexpr`/`const`）
- [ ] 无 TODO 遗留（或已记录 Issue）
- [ ] 线程安全已审查（共享状态加锁）
- [ ] 异常安全已审查（析构函数不抛异常）
- [ ] 资源泄漏已审查（RAII / 智能指针）
- [ ] 无死锁风险（锁顺序一致，无循环等待）
- [ ] 安全相关代码已走安全审查

---

## 8. 工具配置

### 8.1 .clang-format

```yaml
Language: Cpp
BasedOnStyle: Google
IndentWidth: 2
ColumnLimit: 100
AllowShortFunctionsOnASingleLine: Empty
AllowShortIfStatementsOnASingleLine: false
AllowShortLoopsOnASingleLine: false
BreakBeforeBraces: Attach
PointerAlignment: Left
ReferenceAlignment: Left
SortIncludes: true
IncludeBlocks: Regroup
```

### 8.2 CPPLINT.cfg

```cfg
filter=-build/c++11,-build/namespaces,-runtime/references,-whitespace/braces
linelength=100
```

### 8.3 .pylintrc（关键配置）

```ini
[FORMAT]
max-line-length=100
indent-string='    '

[BASIC]
function-naming-style=snake_case
method-naming-style=snake_case
class-naming-style=PascalCase
const-naming-style=UPPER_CASE
attr-naming-style=snake_case

[MESSAGES CONTROL]
disable=C0114,C0115,C0116  # 如果项目使用其他文档工具，可禁用 docstring 检查
```
