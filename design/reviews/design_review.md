# Claude Code 模块设计审查意见

审查对象：

- `agent_design.md`
- `em_design_v2.md`
- `hal_ethercat_design.md`
- `hds_design.md`
- `mc_design.md`
- `pnc_design.md`
- `sm_design.md`
- `te_design.md`

审查依据：

- `CLAUDE.md` 当前架构与并行协作命名规则
- ROS2 接口规范：Service 响应必须包含 `bool accepted/success` + `string message/reason`
- ROS2 Action 规范：Goal 包含 `int32 timeout_sec`；Result 包含 `bool success` + `string message`；Feedback 包含 `float32 progress` + `string phase`
- 安全规则：运动指令必须经过 SM 校验；`ACTIVE_E_STOP` / `FAULT` 下拒绝运动；HDS 只定级不上控制；非 Gateway 不直连云端；EM 是进程控制唯一入口

## 1. 阻塞级问题

### P0-1：TE、EM、HDS 仍使用已废弃的 `ACTIVE_IDLE` / `ACTIVE_BUSY`

当前 SM 设计中没有 `ACTIVE_IDLE` / `ACTIVE_BUSY`，实际状态为 `STANDBY`、`ACTIVE_STAND`、`ACTIVE_READY`、`ACTIVE_MOTION`、`ACTIVE_WALKING` 等 17 状态。但多个设计仍以旧状态作为状态转换目标和恢复目标：

- `te_design.md:9`：声明 TE 将机器人从 `ACTIVE_IDLE` 推进到 `ACTIVE_BUSY`
- `te_design.md:16`、`te_design.md:474`、`te_design.md:598`、`te_design.md:618`：任务开始/结束请求 `ACTIVE_BUSY` / `ACTIVE_IDLE`
- `em_design_v2.md:104`、`em_design_v2.md:439`：SM 映射示例仍是 `ACTIVE_IDLE`
- `hds_design.md:122`、`hds_design.md:123`、`hds_design.md:290`、`hds_design.md:502`：定级映射和恢复检查仍引用 `ACTIVE_IDLE` / `ACTIVE_BUSY`

影响：

- TE、EM、HDS 与 SM 接口无法实现一致状态转换。
- 恢复前检查、任务执行、进程编排会使用不存在的状态值。
- 安全审查无法证明运动任务只在合法 ACTIVE 状态执行。

整改建议：

- TE 任务执行应改为请求具体运动状态：例如动作任务请求 `ACTIVE_MOTION`，导航任务请求 `ACTIVE_WALKING`，结束后回到 `ACTIVE_READY` 或 `ACTIVE_STAND`。
- EM 文档中的状态到进程组映射示例必须改为当前 17 状态。
- HDS 恢复目标仅允许 `STANDBY`、`ACTIVE_STAND` 或 SM 明确定义的恢复态，不允许旧状态。

### P0-2：Agent 设计允许云端 LLM API，违反 Gateway 唯一云端出口

Agent 文档在职责边界中写了“不直接连接云端”，但参数和内部设计允许 Agent 直接访问云端模型：

- `agent_design.md:47`：声明所有云端通信通过 Gateway 转发
- `agent_design.md:390`：支持云端 API 推理
- `agent_design.md:495` 至 `agent_design.md:504`：配置 `provider: local / cloud`、`cloud_api_endpoint`、`cloud_api_key`、`auto_switch_to_cloud`
- `agent_design.md:573`：隐私章节允许 cloud 模式上传用户对话

影响：

- 违反 `CLAUDE.md` “Gateway 是唯一云端通信出口”的红线。
- API Key、用户语音/对话、任务上下文可能绕过 Gateway 的鉴权、审计、脱敏和网络策略。

整改建议：

- 删除 Agent 直连云端配置。
- 如果必须使用云端 LLM，Agent 只能调用 Gateway 或专门的 Gateway 代理接口，由 Gateway 负责鉴权、审计、脱敏、限流和云端连接。
- `cloud_api_key` 不应出现在 Agent 参数文件中。

### P0-3：EM 的 L4 恢复策略在 SM/HDS 决策前停止运动进程，安全链路不闭合

EM 文档强调 L4 只是向 HDS 建议 E-Stop，但同一流程又描述 EM 立即停止运动进程：

- `em_design_v2.md:361`：P0-Critical 失败时 EM “立即停止运动进程”，再等待 HDS 决定是否请求 SM 触发 `ACTIVE_E_STOP`
- `em_design_v2.md:401`：E-Stop Handler 通过 UDS 通知 daemon 停止运动相关进程
- `em_design_v2.md:512`：daemon 停止运动相关进程

影响：

- 如果尚未进入 `ACTIVE_E_STOP`，EM 先杀运动进程可能绕过 MC 的安全收敛路径。
- “停止进程”和“进入安全运动状态”不是等价操作；对运动控制进程直接停止可能导致最后指令保持、刹车时序不明确或下层 HAL 状态不可控。

整改建议：

- L4 建议阶段只允许上报 HDS，不应停止运动进程。
- 真正的紧急路径应为：HDS/硬件/MC/Gateway 请求 SM `ACTIVE_E_STOP`，SM 进入急停后广播状态，MC 先进入安全模式并确认，EM 再停止或重启相关进程。
- EM 可以有“看门狗兜底”，但必须明确在 MC 安全确认失败时的硬件级保护路径，并由 SM 急停态约束。

## 2. 高风险一致性问题

### P1-1：Claude Code 模块内部包名、Topic、Service 仍未使用 `claude_` 前缀

文件名已改为 `claude_*.md`，但文档内部仍大量使用无前缀包名和接口命名，例如：

- `agent_design.md:179`、`agent_design.md:311`：`agent_msgs`、`/agent/...`
- `sm_design.md:202`、`sm_design.md:396`：`sm_msgs`、`/sm/...`
- `te_design.md:150`、`te_design.md:457`：`te_msgs`、`/te/...`
- `mc_design.md:124`、`mc_design.md:316`：`mc_msgs`、`/mc/...`
- `hal_ethercat_design.md:101`、`hal_ethercat_design.md:287`：`hal_ethercat_msgs`、`/hal_ethercat/...`

影响：

- 与 `CLAUDE.md` 中的并行协作命名规则冲突。
- 后续 Codex/Claude Code 并行设计时，包名和接口名仍可能冲突。

整改建议：

- Claude Code 设计产物统一迁移为 `claude_{module}_msgs`、`claude_{module}`、`/claude_{module}/...`。
- 跨模块引用也应同步成对方实际命名，或在文档中明确“历史接口名待迁移”。
- ROS2 标准接口可保留原名；项目自定义接口不应保留无前缀形式。

### P1-2：多个 Service 响应缺少 `success/accepted` 与 `message/reason`

不符合 ROS2 接口规范，主要集中在查询类和健康类服务：

- `agent_design.md:252` 至 `agent_design.md:260`：`QueryStatus.srv` 无 `bool success` / `string message`
- `agent_design.md:273` 至 `agent_design.md:282`：`GetHealthStatus.srv` 无 `success/message`
- `te_design.md:363` 至 `te_design.md:376`：`GetHealthStatus.srv` 无 `success/message`
- `sm_design.md:304` 至 `sm_design.md:312`：`GetState.srv` 无 `success/message`
- `sm_design.md:376` 至 `sm_design.md:386`：`GetHealthStatus.srv` 无 `success/message`
- `pnc_design.md:219` 至 `pnc_design.md:227`：`GetHealthStatus.srv` 无 `success/message`
- `mc_design.md:274` 至 `mc_design.md:283`：`GetHealthStatus.srv` 无 `success/message`
- `hal_ethercat_design.md:268` 至 `hal_ethercat_design.md:278`：`GetHealthStatus.srv` 无 `success/message`
- `hds_design.md:254` 至 `hds_design.md:270`：`QueryDiagnosis.srv` 无 `success/message`
- `hds_design.md:289` 至 `hds_design.md:298`：`RecoveryCheck.srv` 使用 `allowed/message`，但缺少 `success`
- `hds_design.md:303` 至 `hds_design.md:308`：`GetSystemHealth.srv` 无 `success/message`
- `hds_design.md:316` 至 `hds_design.md:330`：`RegisterHealthEntity.srv` 缺少 `message`

影响：

- 调用方无法区分“查询失败”和“查询成功但返回 unhealthy/empty”。
- 代码生成后异常处理风格不统一。

整改建议：

- 所有 Service Response 统一增加 `bool success` 或 `bool accepted`，以及 `string message` 或 `string reason`。
- 业务布尔值如 `healthy`、`allowed`、`found` 不能替代 RPC 执行结果。

### P1-3：Action 定义不符合项目规范

项目规范要求 Action：

- Goal：包含 `int32 timeout_sec`
- Result：包含 `bool success` + `string message`
- Feedback：包含 `float32 progress` + `string phase`

发现问题：

- `agent_design.md:288` 至 `agent_design.md:303`：`ExecuteInteraction.action` Goal 无 `timeout_sec`；Feedback 无 `progress`，使用 `current_phase`
- `te_design.md:400` 至 `te_design.md:446`：TE Action 使用 `builtin_interfaces/Duration max_execution_time`，Feedback 使用 `progress_percent` / `overall_progress` 和 `current_phase`
- `mc_design.md:290` 至 `mc_design.md:307`：`ExecuteMotion.action` Goal 无 `timeout_sec`，Feedback 使用 `progress_percent/current_phase`
- `pnc_design.md:233` 至 `pnc_design.md:255`：`NavigateTo.action` 使用 `float64 timeout_sec`，Feedback 使用 `progress_percent/current_state`
- `em_design_v2.md:263` 至 `em_design_v2.md:276`：`SystemRestart.action` Goal 无 `timeout_sec`，Feedback 使用 `progress_percent/current_action`

影响：

- Action client 通用封装无法复用统一字段。
- 进度语义 0-1 与 0-100 混用，容易造成 UI/调度错误。

整改建议：

- 全部 Action Goal 增加 `int32 timeout_sec`。
- Feedback 统一使用 `float32 progress`，取值 0.0-1.0；阶段字段统一为 `string phase`。
- 原来的 `progress_percent/current_phase/current_state` 可作为额外字段保留，但不能替代规范字段。

### P1-4：HDS 与 EM 的 MQTT Topic 名称不一致

EM v2 定义了拆分后的 MQTT topic：

- `em_design_v2.md:299` 至 `em_design_v2.md:302`：`em/event/l3_suggestion` 与 `em/event/l4_suggestion`

HDS 交互矩阵仍引用合并 topic：

- `hds_design.md:538`：`em/event/l3_l4_suggestion`
- `hds_design.md:905`：文档自身也记录了该不一致

影响：

- HDS 无法收到 EM 的 L3/L4 建议，或实现时需要额外兼容逻辑。

整改建议：

- HDS 文档改为订阅 `em/event/l3_suggestion` 与 `em/event/l4_suggestion`。
- 如果需要兼容旧 topic，必须明确迁移期和优先级。

### P1-5：HDS 恢复前检查职责与 SM/EM 边界存在循环

HDS 文档描述恢复前检查会检查 EM 核心进程：

- `hds_design.md:502` 至 `hds_design.md:504`：检查 P0/P1 模块、EM 核心进程、SM 自身
- `hds_design.md:886` 至 `hds_design.md:891`：文档自己的审查段落也指出应避免 SM → HDS → EM 间接循环

影响：

- 恢复链路边界不清，SM、HDS、EM 互相调用会增加死锁或超时风险。
- HDS 只应根据健康数据给出诊断，不应替 SM 直接检查 EM 进程状态。

整改建议：

- SM 恢复前检查拆成两部分：SM 直接调用 EM 查询进程状态；SM 调用 HDS 查询健康诊断结果。
- HDS `RecoveryCheck` 只返回诊断实体是否恢复，不检查 EM 运行态。

## 3. 中风险设计问题

### P2-1：TE 设计中存在“后台启动训练进程”，违反进程控制边界

TE 明确声明不直接操作进程，但任务执行流程中写了：

- `te_design.md:46`：TE 不直接操作进程
- `te_design.md:612`：模型训练任务“后台启动训练进程”

影响：

- 与 EM 是唯一进程控制入口的规则冲突。

整改建议：

- TE 只能通过 EM Service 请求训练进程/任务依赖进程启动。
- 更建议训练任务由 DR/Agent/专门训练模块提供 Action，TE 只调度 Action。

### P2-2：EM 的 ROS2 IDL 片段使用非标准分隔符

EM 服务和 Action 示例使用 `--- 请求 ---`、`--- 响应 ---`、`--- 目标 ---` 等形式：

- `em_design_v2.md:187`、`em_design_v2.md:196`
- `em_design_v2.md:221`、`em_design_v2.md:226`
- `em_design_v2.md:263` 至 `em_design_v2.md:276`

影响：

- 这些片段不能直接作为 `.srv` / `.action` 文件内容。

整改建议：

- `.srv` 中只使用单独一行 `---` 分隔 Request/Response。
- `.action` 中只使用两行 `---` 分隔 Goal/Result/Feedback，并将中文说明放到注释中。

### P2-3：HDS 文档内存在 Markdown 代码块断裂

`hds_design.md` 中 `GetSystemHealth.srv` 后紧接 `RegisterHealthEntity.srv`，中间缺少清晰的代码块闭合/打开：

- `hds_design.md:303` 至 `hds_design.md:316`

影响：

- 读者容易误以为两个 Service 属于同一代码块。
- 后续从文档抽取接口定义时容易出错。

整改建议：

- 每个 `.srv` 用独立 fenced code block。
- 对所有设计文档做一次 Markdown fenced block 校验。

### P2-4：包结构仍是旧命名，未体现 Claude Code 命名空间

包结构章节仍使用无前缀目录：

- `agent_design.md:581`、`agent_design.md:597`
- `sm_design.md:680`、`sm_design.md:697`
- `te_design.md:864`、`te_design.md:886`
- `hds_design.md:747`、`hds_design.md:766`
- `mc_design.md:704` 至 `mc_design.md:720`
- `pnc_design.md:621` 至 `pnc_design.md:637`
- `hal_ethercat_design.md:577` 至 `hal_ethercat_design.md:596`
- `em_design_v2.md:669`、`em_design_v2.md:685`

影响：

- 与当前并行协作规则不一致。
- 生成代码时容易创建无前缀包，导致与 Codex 或第三方包冲突。

整改建议：

- 统一改为 `claude_{module}_msgs/` 和 `claude_{module}/`。

## 4. 模块级整改建议

### Agent

优先整改：

1. 删除直连云端 LLM API 配置，改为 Gateway 代理。
2. `QueryStatus`、`GetHealthStatus` 补齐 `success/message`。
3. `ExecuteInteraction.action` 增加 `timeout_sec`、`progress`、`phase`。
4. 内部包名、Topic、Service 迁移到 `agent_*` / `/agent/...`。

### EM

优先整改：

1. 去除 `ACTIVE_IDLE` 示例，改为当前 SM 17 状态。
2. L4 不应在 SM/HDS 决策前停止运动进程。
3. `.srv` / `.action` 示例改为标准 ROS2 IDL。
4. 明确 `/em/estop_immediate` 与 SM `ACTIVE_E_STOP` 的先后关系：只能作为 SM 急停广播后的执行路径，不能替代 SM 决策。
5. 包名、接口名迁移到 `em_*` / `/em/...`。

### HDS

优先整改：

1. 删除 `ACTIVE_IDLE` / `ACTIVE_BUSY`。
2. 修正 EM MQTT topic 名称。
3. 恢复前检查不检查 EM 进程状态。
4. 所有查询类 Service 补齐 `success/message`。
5. 修复代码块断裂。
6. 包名、接口名迁移到 `hds_*` / `/hds/...`。

### SM

优先整改：

1. `GetState`、`IsMotionAllowed`、`GetHealthStatus` 补齐 `success/message`。
2. 重新确认 `IsMotionAllowed` 是否应允许 `ACTIVE_ZERO_TORQUE` / `ACTIVE_DAMPING` 下的任意“运动指令”；建议细分为“模式切换允许”和“主动运动允许”。
3. 包名、接口名迁移到 `sm_*` / `/sm/...`，或在过渡期明确 SM 是历史核心接口例外。

### TE

优先整改：

1. 删除 `ACTIVE_IDLE` / `ACTIVE_BUSY`，按任务类型请求 `ACTIVE_MOTION` / `ACTIVE_WALKING` 等当前状态。
2. 不直接后台启动训练进程，改为调用 EM 或专门模块 Action。
3. Action 字段按项目规范统一。
4. `GetHealthStatus` 补齐 `success/message`。
5. 包名、接口名迁移到 `te_*` / `/te/...`。

### MC

优先整改：

1. `GetHealthStatus` 补齐 `success/message`。
2. `ExecuteMotion.action` 增加 `timeout_sec`、`progress`、`phase`。
3. 包名、接口名迁移到 `mc_*` / `/mc/...`。

### PnC

优先整改：

1. `GetHealthStatus` 补齐 `success/message`。
2. `NavigateTo.action` 的 `timeout_sec` 改为 `int32 timeout_sec`，Feedback 增加 `progress` 和 `phase`。
3. 包名、接口名迁移到 `pnc_*` / `/pnc/...`。

### EtherCAT HAL

优先整改：

1. `GetHealthStatus` 补齐 `success/message`。
2. 包名、接口名迁移到 `claude_ethercat_hal_*` / `/hal_ethercat/...`。
3. 确认非 OPERATIONAL 状态下“静默丢弃关节指令”是否足够可观测；建议发布明确错误计数或状态事件，避免 MC/HDS 误判。

## 5. 建议整改顺序

1. 先统一状态模型：修复 TE、EM、HDS 的 `ACTIVE_IDLE` / `ACTIVE_BUSY`。
2. 再修安全边界：Agent 云端直连、EM L4 停运动进程、TE 后台启动训练进程。
3. 再修接口规范：Service 响应、Action 字段、EM IDL 语法。
4. 再修命名空间：内部包名、Topic、Service、参数和 launch 文件迁移到 `claude_` 前缀。
5. 最后做文档机械校验：Markdown code fence、接口汇总表、包结构、QoS 完整性。

