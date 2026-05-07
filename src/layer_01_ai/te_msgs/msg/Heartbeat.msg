builtin_interfaces/Time stamp
string node_name                   # 节点名称（固定 "task_engine"）
uint8 state                        # 调度器状态
bool healthy
string status_message
uint32 active_task_count           # 活跃任务数
uint32 total_tasks_this_session    # 本次会话累计处理任务数
bool sm_connected                  # SM 连接状态
