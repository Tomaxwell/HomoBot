uint8 AGENT_IDLE       = 0
uint8 AGENT_THINKING   = 1   # 正在推理
uint8 AGENT_EXECUTING  = 2   # 正在执行技能
uint8 AGENT_WAITING    = 3   # 等待用户确认/等待任务完成
uint8 AGENT_ERROR      = 4   # 出错

uint8 state
string current_skill             # 当前正在执行的技能
string current_task_id           # 当前关联的任务
float32 progress_percent         # 0.0~100.0
builtin_interfaces/Time stamp
