---
name: safety-rules
description: 人形机器人端侧软件安全约束规则。涉及运动控制、状态机、E-Stop的设计和实现时自动加载。
user-invocable: false
paths: "**/mc*,**/ms*,**/mp*,**/sm*,**/pnc*"
---

# 人形机器人端侧软件安全约束

## 绝对禁止（红线）

以下操作在任何情况下都不允许：

1. **在 `ACTIVE_E_STOP` 或 `FAULT` 状态下发送任何运动指令**
   - MC、MS、MP 在这两个状态下必须拒绝所有运动命令
   - 拒绝时返回错误码 `ESTOP_ACTIVE` 或 `FAULT_UNACKNOWLEDGED`

2. **软件自动解除急停**
   - `ACTIVE_E_STOP → ACTIVE_STAND` 或其他恢复态转换必须由人工通过 Gateway 确认
   - 不允许超时自动解除、任务完成自动解除

3. **绕过 SM 直接控制运动**
   - 所有运动指令必须经过 SM 状态校验
   - 不允许 MC/MS/MP 内部自行判断当前状态后执行运动

4. **非 EM 模块启停其他进程**
   - 只有 EM 可以调用 `subprocess`、`systemctl`、`kill` 等进程控制 API

5. **非 Gateway 模块直连云端**
   - 禁止其他模块直接调用云端 API 或建立长连接

## 必须实现（强制项）

### 所有运动相关模块（MC、MS、MP、PnC）

```cpp
// 每个运动指令回调必须包含此检查
bool is_motion_allowed() {
  auto state = sm_client_->get_current_state();
  return state != RobotState::FAULT
      && state.sub_state != RobotState::ACTIVE_E_STOP;
}
```

### SM 模块

- 急停请求 `priority` 固定为 100（最高优先级）
- 所有转换请求必须记录 `requester_node` 和 `reason`
- `BOOTING` 超时（默认 30s）必须进入 `FAULT`，不得进入 `STANDBY`

### EM 模块

- 重启策略必须有熔断：5分钟内3次重启失败 → 停止重试 + 通知 HDS
- P0 模块崩溃必须触发 `FAULT`
- 进程崩溃检测延迟 < 500ms

### HDS 模块

- 只负责故障定级和上报，不直接控制运动
- 通过 `/sm/request_transition` 请求状态转换，不直接修改状态

## 优先级规范

| 操作类型 | priority 值 | 说明 |
|----------|-------------|------|
| 急停（E-Stop）| 100 | 最高，不可被覆盖 |
| 安全降级 | 80 | HDS 触发的故障响应 |
| 系统操作 | 60 | FOTA、关机等 |
| 任务操作 | 40 | TE 控制的任务 |
| 用户操作 | 20 | Gateway 转发的用户指令 |

## 故障恢复流程

```
FAULT 状态
  ↓ (只有这一条路)
AcknowledgeFault service（operator_id 必填）
  ↓
SM 验证操作者权限
  ↓
恢复前检查（硬件状态、进程状态）
  ↓
FAULT → STANDBY（如检查通过）
或
FAULT → SHUTTING_DOWN（如操作者选择关机）
```
