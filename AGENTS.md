# AGENTS.md

## 文件职责

`AGENTS.md` 是 Codex CLI 在本仓库的主指令文件。Codex 应以本文件作为本地工作指南。

`CLAUDE.md` 是 Claude Code 的主指令文件。Codex 可以读取它来了解共享架构上下文和设计索引，但除非用户明确要求，Codex 不应改写 Claude Code 专属工作规则。

## 仓库概述

本仓库包含人形机器人端侧软件系统的 ROS2 设计与实现资产。核心模块包括 SM、EM、Gateway、TE、HDS、Agent、MC、MP、MS、PnC、Perception、VSLAM、Lidar-SLAM、MapManager、DR、FOTA、Setting、RC、HAL_EtherCAT 和 TF。

## 核心架构规则

1. SM 是机器人全局状态的唯一权威。
2. EM 是唯一允许启动、停止、重启或监控进程的组件。
3. Gateway 是唯一的云端或 APP 通信出口。
4. HDS 负责故障定级；其他模块只上报原始事实。
5. 所有运动指令执行前必须通过 SM 状态校验。

## 并行设计归属

- Codex 负责的新设计文件使用 `codex_` 前缀：`codex_{module}_design.md`、`codex_{module}_msgs`、`codex_{module}`、`/codex_{module}/...`。
- Claude Code 负责的设计文件使用 `claude_` 前缀：`claude_{module}_design.md`、`claude_{module}_msgs`、`claude_{module}`、`/claude_{module}/...`。
- 不要重命名、覆盖或删除另一个工具前缀下的产物。
- 更新 `CLAUDE.md` 时，Codex 可以维护共享设计索引和共享架构规则，但必须保留 Claude Code 专属工作流说明。
- Codex 专属工作流和配置写入 `AGENTS.md` 与 `.codex/`；Claude Code 专属工作流和配置写入 `CLAUDE.md` 与 `.claude/`。
- ROS 标准接口保持规范原名，例如 `/tf`、`/tf_static`、`/joint_states` 和 `robot_description`。

## 安全规则

- 禁止在 `FAULT` 或 `ACTIVE_E_STOP` 状态下发送运动指令。
- 禁止软件自动解除急停；解除必须经过 Gateway 人工确认。
- 禁止在运动执行前绕过 `/sm/is_motion_allowed` 或等价的 SM 校验。
- 非 EM 模块禁止使用 `subprocess`、`systemctl`、`kill` 等进程控制 API。
- 非 Gateway 模块禁止直接调用云端 API 或建立云端长连接。

## ROS2 接口规则

- 消息名使用 PascalCase；字段名使用 `snake_case`；枚举常量使用 `UPPER_SNAKE_CASE`。
- Service 必须包含 `bool accepted` 或 `bool success`，以及 `string message` 或 `string reason`。
- Action 的 Goal 必须包含 `int32 timeout_sec`；Result 必须包含 `bool success` 和 `string message`；Feedback 必须包含 `float32 progress` 和 `string phase`。
- 状态广播使用 Reliable + Transient Local + Keep Last 1 QoS。
- 传感器流使用 Best Effort + Volatile QoS。
- 命令/控制链路使用 Reliable + Volatile QoS。

## 工作流

- Codex 新模块设计遵循 `.codex/commands/module-design.md`。
- ROS2 接口生成遵循 `.codex/commands/ros2-interface.md`。
- 设计审查遵循 `.codex/commands/design-review.md`。
- 代码审查遵循 `.codex/commands/review.md`。
- 搜索优先使用 `rg` / `rg --files`。不要回退与当前任务无关的用户或 Claude Code 改动。
