# Codex Project Guide: Humanoid Robot Edge Software

This project also has a root `AGENTS.md`; prefer that file for current Codex guidance. This `.codex/AGENTS.md` is kept as local reference for Codex-specific commands, skills, and agents.

## Project Overview

This repository is a ROS2-based humanoid robot edge-side software system. It contains the core modules running on the robot body computer:

SM, EM, Gateway, TE, HDS, Agent, MC, MP, MS, PnC, Perception, VSLAM, Lidar-SLAM, MapManager, DR, FOTA, Setting, RC, HAL_EtherCAT, and TF.

## Core Architecture Rules

1. SM is the only global robot state authority. Other modules may read state but must not mutate it directly.
2. EM is the only component allowed to start, stop, restart, or supervise other processes.
3. Gateway is the only cloud or app communication exit.
4. HDS owns fault classification. Other modules report raw anomalies instead of making business-level fault decisions.
5. All motion commands must pass SM state validation before execution.

## Parallel Design Ownership

- Codex owns `codex_*` design files, packages, and custom interfaces.
- Claude Code owns `claude_*` design files, packages, and custom interfaces.
- Do not rename, overwrite, or delete the other tool's prefixed artifacts.

## Safety Rules

- Never send motion commands in `FAULT` or `ACTIVE_E_STOP`.
- Never automatically clear E-Stop in software. E-Stop release requires manual Gateway confirmation.
- Never bypass `/sm/is_motion_allowed` or equivalent SM validation before motion execution.
- Non-EM modules must not use process-control APIs such as `subprocess`, `systemctl`, or `kill`.
- Non-Gateway modules must not directly call cloud APIs or establish cloud long connections.

## ROS2 Conventions

- Message files use PascalCase, for example `RobotState.msg`.
- ROS2 fields use `snake_case`.
- Enum constants use `UPPER_SNAKE_CASE`.
- Services should include `bool accepted` or `bool success`, plus `string message` or `string reason`.
- Actions should include `int32 timeout_sec` in Goal, `bool success` and `string message` in Result, and `float32 progress` plus `string phase` in Feedback.
- State broadcasts use Reliable + Transient Local + Keep Last 1 QoS.
- Sensor streams use Best Effort + Volatile QoS.
- Command and control paths use Reliable + Volatile QoS.

## Design Workflow

- New module design follows `.codex/commands/module-design.md`.
- ROS2 interface generation follows `.codex/commands/ros2-interface.md`.
- Code review follows `.codex/commands/review.md`.
- Design review follows `.codex/commands/design-review.md`.
- Delivery verification follows `.codex/commands/go.md`.

## Synced Skills

- `.codex/skills/ros2-conventions/SKILL.md`
- `.codex/skills/safety-rules/SKILL.md`
- `.codex/skills/module-design-template/SKILL.md`

## Synced Specialist Prompts

- `.codex/agents/architecture-advisor.md`
- `.codex/agents/code-reviewer.md`
- `.codex/agents/module-designer.md`
- `.codex/agents/ros2-interface-definer.md`
- `.codex/agents/safety-validator.md`

## MCP

The project MCP server synced from `.mcp.json` is configured in `.codex/config.toml`:

- `context7`: `npx -y @upstash/context7-mcp@latest`
