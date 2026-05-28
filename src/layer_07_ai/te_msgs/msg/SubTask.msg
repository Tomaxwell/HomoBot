uint8 SUB_TYPE_MOTION_ACTION    = 1   # 调用 MP Action
uint8 SUB_TYPE_NAV_ACTION       = 2   # 调用 PnC Action
uint8 SUB_TYPE_INTERACTION      = 3   # 调用 Agent Service/Action
uint8 SUB_TYPE_WAIT             = 4   # 等待条件
uint8 SUB_TYPE_PARALLEL         = 5   # 并行执行子任务组
uint8 SUB_TYPE_CONDITIONAL      = 6   # 条件分支

string subtask_id                  # 子任务 ID
uint8 subtask_type                 # 子任务类型
string target_module               # 目标模块（mp/pnc/agent）
string action_or_service_name      # 调用的 Action/Service 名称
string parameters_json             # 参数 JSON 字符串
string[] depends_on                # 依赖的子任务 ID 列表
int32 timeout_sec                  # 子任务超时（秒），-1 表示无超时
