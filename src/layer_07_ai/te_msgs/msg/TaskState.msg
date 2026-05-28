uint8 TE_TASK_CREATED     = 0
uint8 TE_TASK_SCHEDULED   = 1
uint8 TE_TASK_PREPARING   = 2
uint8 TE_TASK_RUNNING     = 3
uint8 TE_TASK_PAUSED      = 4
uint8 TE_TASK_CANCELLING  = 5
uint8 TE_TASK_COMPLETED   = 6
uint8 TE_TASK_FAILED      = 7
uint8 TE_TASK_CANCELLED   = 8

uint8 TE_TYPE_UNKNOWN     = 0
uint8 TE_TYPE_MOTION      = 1    # 运动任务（调用 MP/MC/MS）
uint8 TE_TYPE_NAVIGATION  = 2    # 导航任务（调用 PnC）
uint8 TE_TYPE_INTERACTION = 3    # 交互任务（调用 Agent）
uint8 TE_TYPE_COMPOSITE   = 4    # 复合任务（多子任务编排）
uint8 TE_TYPE_EMERGENCY   = 5    # 紧急任务（最高优先级）
uint8 TE_TYPE_TELEOP     = 6    # 遥操作任务（调用 MC）
uint8 TE_TYPE_RECORD     = 7    # 数据录制任务（调用 DR + MC）
uint8 TE_TYPE_REPLAY     = 8    # 动作回放任务（调用 MP）
uint8 TE_TYPE_INFER      = 9    # VLA 推理任务（调用 MC RL 策略）
uint8 TE_TYPE_CALIBRATE  = 10   # 校准任务（调用 MC）
uint8 TE_TYPE_TRAIN      = 11   # 模型训练任务（后台离线，调用外部资源）

string task_id                     # 任务唯一标识（UUID）
string parent_task_id              # 父任务 ID（子任务时有效，空字符串表示根任务）
uint8 task_type                    # 任务类型
uint8 state                        # 当前任务状态
uint8 previous_state               # 上一个状态
builtin_interfaces/Time created_at # 创建时间
builtin_interfaces/Time started_at # 开始执行时间
builtin_interfaces/Time ended_at   # 结束时间
float32 progress_percent           # 执行进度 0.0~100.0
string current_subtask             # 当前执行的子任务名称
uint16 error_code                  # 失败时的错误码（ErrorCode）
string error_message               # 失败时的错误描述
string requester_node              # 请求者节点名
