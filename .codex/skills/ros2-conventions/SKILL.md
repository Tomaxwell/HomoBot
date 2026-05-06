---
name: ros2-conventions
description: ROS2 编码规范和接口设计规则。当编写或审查 ROS2 接口定义、节点代码时自动加载。
user-invocable: false
---

# ROS2 项目编码规范

## 接口命名规范

### 消息文件（.msg）
- 文件名：PascalCase，如 `RobotState.msg`、`ModuleReadiness.msg`
- 字段名：snake_case，如 `entered_at`、`transition_count`
- 枚举常量：UPPER_SNAKE_CASE，如 `uint8 BOOTING = 0`
- 枚举常量在同类字段前声明

### 服务文件（.srv）
- 文件名：动词+名词，如 `RequestTransition.srv`、`GetState.srv`
- Request 部分：请求参数
- Response 部分：必含 `bool accepted/success` + `string message/reason`

### Action 文件（.action）
- Goal：目标参数 + `int32 timeout_sec`
- Result：`bool success` + `string message` + 结果数据
- Feedback：`float32 progress`（0~1）+ `string phase`（阶段描述）

## Topic 命名规范

Codex 新设计模块格式：`/codex_{module_lower}/{topic_name}`。
Claude Code 新设计模块格式：`/claude_{module_lower}/{topic_name}`。
历史接口和 ROS2 标准接口按既有名称保留，例如 `/tf`、`/tf_static`、`/joint_states`。

| 模块 | Topic 示例 |
|------|-----------|
| codex_tf | `/codex_tf/tf_state`、`/codex_tf/heartbeat` |
| claude_sm | `/claude_sm/robot_state`、`/claude_sm/transition_event` |
| 历史 sm | `/sm/robot_state`、`/sm/request_transition` |

## QoS 配置模板（C++）

```cpp
// 状态广播（新订阅者获取最新值）
auto qos_state = rclcpp::QoS(1)
    .reliable()
    .transient_local();

// 传感器高频数据
auto qos_sensor = rclcpp::QoS(10)
    .best_effort()
    .volatile_();

// 命令/控制
auto qos_cmd = rclcpp::QoS(10)
    .reliable()
    .volatile_();
```

## 节点类模板（C++）

```cpp
class ModuleNameNode : public rclcpp::Node {
public:
  explicit ModuleNameNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions())
  : Node("module_name_node", options)
  {
    // 1. 声明参数
    this->declare_parameter("param_name", default_value);

    // 2. 创建 CallbackGroup（隔离阻塞操作）
    cb_group_service_ = this->create_callback_group(
        rclcpp::CallbackGroupType::MutuallyExclusive);

    // 3. 创建 Publisher/Subscriber/Service/Client
    // ...

    RCLCPP_INFO(this->get_logger(), "ModuleNameNode initialized");
  }

private:
  rclcpp::CallbackGroup::SharedPtr cb_group_service_;
  // ...
};
```

## 错误处理规范

```cpp
// Service 回调中必须捕获异常
void handle_service(
  const std::shared_ptr<ModuleMsgs::srv::DoThing::Request> request,
  std::shared_ptr<ModuleMsgs::srv::DoThing::Response> response)
{
  try {
    // 业务逻辑
    response->accepted = true;
    response->message = "success";
  } catch (const std::exception & e) {
    response->accepted = false;
    response->message = e.what();
    RCLCPP_ERROR(this->get_logger(), "Service failed: %s", e.what());
  }
}
```

## 包结构规范

```
codex_{module}_msgs/
├── msg/
│   ├── XxxState.msg
│   └── XxxRequest.msg
├── srv/
│   ├── DoXxx.srv
│   └── GetXxx.srv
├── action/
│   └── XxxTo.action
├── CMakeLists.txt
└── package.xml

codex_{module}/
├── include/codex_{module}/
│   ├── {module}_node.hpp
│   └── internal_class.hpp
├── src/
│   ├── {module}_node.cpp
│   └── internal_class.cpp
├── test/
│   └── test_codex_{module}.cpp
├── config/
│   └── codex_{module}_params.yaml
├── launch/
│   └── codex_{module}.launch.py
├── CMakeLists.txt
└── package.xml
```
