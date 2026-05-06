uint8 TE_SCHEDULER_INIT       = 0
uint8 TE_SCHEDULER_IDLE       = 1
uint8 TE_SCHEDULER_BUSY       = 2
uint8 TE_SCHEDULER_PAUSED     = 3
uint8 TE_SCHEDULER_DEGRADED   = 4
uint8 TE_SCHEDULER_FAULT      = 5

uint8 state                        # 调度器状态
string current_task_id             # 当前执行中的任务 ID
uint32 queue_depth                 # 任务队列深度
builtin_interfaces/Time state_entered_at  # 进入当前状态的时间
