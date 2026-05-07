# SM 模块设计审查 — 待办项跟踪

**关联文档**：[sm_design.md §11 设计审查记录](sm_design.md#11-设计审查记录)
**审查日期**：2026-05-07
**总体判决**：APPROVED_WITH_CONDITIONS
**HIGH 项总数**：8（4 安全 + 4 架构）

> 本文件用于跟踪 §11 审查报告产出的修复事项的执行状态。审查报告本身是只读快照，本文件是活动跟踪。

---

## 推进顺序总览

```
Wave 1（独立，可并行）         Wave 2（有依赖）
────────────────────         ─────────────────
#6  estop_hardware_design     #8  CLAUDE.md         ← #7
#7  em_design Tier-0          #11 hds_design        ← #7
#13 sm_design 内部闭环         #12 fota_design       ← #7
#9  ACL 机制决策（待用户输入）  #10 voice_subsystem   ← #13
```

---

## Wave 1：独立任务

### `[ ]` #6 — [P0] 新建 `design/layer_07_hal_infra/estop_hardware_design.md`

**审查项**：A-H2
**目标**：硬件急停链路设计文档（当前缺失）
**覆盖**：
- 硬件链路（GPIO IRQ + UART）
- 上电自检机制
- 断电保持行为
- 与 SM `/sm/trigger_estop`（source=`hardware_estop`）的契约
- 失效安全（fail-safe）行为

**建议流程**：走 `/module-design` 标准 10 节结构

---

### `[ ]` #7 — [P0] 更新 `em_design_v2.md` 增补 Tier-0 安全旁路服务清单

**审查项**：A-H1
**目标**：解决 EM 进程治理与 systemd Tier-0 服务的边界冲突
**新增内容**：
- 新增 §"Tier-0 安全旁路服务"小节
- 列举服务清单：`estop_voice_service`、`estop_hardware_service`
- 说明例外条件（为何允许绕过 EM）
- 与 EM 健康观测/重启策略的关系

**关键路径**：解锁 #8 / #11 / #12 三个下游任务

---

### `[ ]` #13 — [P0] sm_design.md 内部 HIGH 项闭环

**审查项**：S-H1 / S-H3 / S-H4 / A-H3（不依赖跨模块决策的部分）

| 子项 | 修改位置 | 内容 |
|------|---------|------|
| S-H1 | §3.2 / §7 | 增补 `ERR_SM_INVALID_KEYWORD` 错误码 + `keyword_id` 二次校验说明 |
| S-H3 | §8.5 | 增补 Release 阶段多源处理规则（"全部触发源清空才允许解除"） |
| S-H4 | §4.3.2 / §10 | 增补重复触发抑制策略 + KWS 误触率纳入 KPI |
| A-H3 | §3.2 | 引入 `keyword_id_version: uint8` 字段 |

**注**：S-H2 文档化（§8.5 ACL 机制说明）依赖 #9 决策完成

---

### `[决策]` #9 — 选定 service ACL 机制

**审查项**：S-H2
**类型**：架构决策（需用户输入）

**两个候选**：

| 方案 | 优点 | 缺点 |
|------|------|------|
| DDS Security（governance.xml + permissions.xml） | ROS2 通用、跨主机、可覆盖所有 service ACL | 配置复杂、调试门槛高、性能微开销 |
| systemd socket + Unix domain socket 隔离 | 部署简单、零运行时开销 | 仅本机调用、不适合后续多机扩展 |

**决策影响**：
- sm_design.md §8.5 文档化方向
- estop_voice_service 实现路径
- 是否需要新增 DDS 配置文件 / systemd socket 单元

---

## Wave 2：依赖任务

### `[ ]` #8 — [P0] CLAUDE.md 补 Tier-0 例外条件 *(blocked by #7)*

**审查项**：A-H1
**修改位置**：`CLAUDE.md` "已知错误（禁止重犯）" 段
**操作**：在 "禁止非 EM 模块启停其他进程" 一条下补充例外说明，引用 em_design_v2.md 的 Tier-0 服务清单

---

### `[ ]` #11 — [P1] hds_design.md 增补 systemd 服务健康观测路径 *(blocked by #7)*

**审查项**：A-H4
**目标**：HDS 必须能观测到 Tier-0 服务的健康状态
**新增内容**：
- 对 `estop_voice_service` / `estop_hardware_service` 的健康观测路径
- 心跳订阅（即便它们不归 EM 管）
- 系统级状态检查（`systemctl is-active`、内存占用、关键词识别延迟）

---

### `[ ]` #12 — [P2] fota_design.md FOTA 期间 Tier-0 服务处置 *(blocked by #7)*

**审查项**：M-A2
**目标**：明确 FOTA 升级期间 Tier-0 服务的处置策略
**需回答的问题**：
- Tier-0 服务是否随系统升级？
- 升级期间 voice 急停链路是否仍然有效？
- 硬件急停链路在 FOTA 期间应始终保持
- 升级时序与回滚边界

---

### `[ ]` #10 — [P1] voice_interaction_subsystem.md 反向引用 + keyword_id_version *(blocked by #13)*

**审查项**：A-H3 + M-A1
**修改位置**：`subsystem/voice_interaction_subsystem.md`
**两项变更**：
1. 反向引用 sm_design.md §3.2 VoiceEStop.srv 与 §8.5 仲裁规则
2. 与 sm_design.md 同步引入 `keyword_id_version` 字段（初值 1，KWS 模型每次新增关键词递增；SM 端只接受 SDK 已知版本）

---

## HIGH 修复项 — 全表

> 8 项 HIGH 必须在合入实现前完成。MEDIUM/LOW 项见 [sm_design.md §11.5](sm_design.md#115-综合修复建议按优先级排序)。

| # | 编号 | 类别 | 一句话风险 | 关联任务 |
|---|------|------|-----------|---------|
| 1 | S-H1 | 安全 | SM 未对 `keyword_id` 二次校验 → KWS 被劫持可任意触发 | #13 |
| 2 | S-H2 | 安全 | 默认 DDS 无 ACL → service 调用方身份不可强制 | #9 + 后续 |
| 3 | S-H3 | 安全 | 100ms 仲裁窗口未规定 Release 阶段策略 → 半解除态风险 | #13 |
| 4 | S-H4 | 安全 | "voice 不允许丢弃" + 无频控 → KWS 误识别可造成抖动 DoS | #13 |
| 5 | A-H1 | 架构 | systemd Tier-0 服务与 EM 治理冲突未文档化 | #7 + #8 |
| 6 | A-H2 | 架构 | `estop_hardware_design.md` 缺失 | #6 |
| 7 | A-H3 | 架构 | `keyword_id` 枚举无版本管理 → 模型升级时合约漂移 | #13 + #10 |
| 8 | A-H4 | 架构 | `estop_voice_service` 缺心跳 / HDS 观测路径 | #11 |

---

## 状态约定

- `[ ]` 待办（pending）
- `[~]` 进行中（in_progress）
- `[x]` 已完成（completed）
- `[!]` 阻塞（blocked，等用户决策或外部输入）

完成一项后：在条目前更新状态、补充实际修改文件路径与日期，例如：

```
### `[x]` #7 — [P0] 更新 em_design_v2.md... ✅ 2026-05-10
**实际修改**：design/layer_06_middleware/em_design_v2.md §3.7
```
