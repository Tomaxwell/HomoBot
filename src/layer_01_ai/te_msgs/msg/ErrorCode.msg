uint16 OK                               = 0
uint16 ERR_INVALID_TASK_TYPE            = 4001   # 不支持的任务类型
uint16 ERR_INVALID_PARAMETERS           = 4002   # 任务参数校验失败
uint16 ERR_SM_TRANSITION_REJECTED       = 4003   # SM 状态转换被拒绝
uint16 ERR_MOTION_NOT_ALLOWED           = 4004   # SM 运动许可校验失败
uint16 ERR_SUBTASK_TIMEOUT              = 4005   # 子任务执行超时
uint16 ERR_SUBTASK_FAILED               = 4006   # 子任务执行失败
uint16 ERR_TASK_CANCELLED               = 4007   # 任务被取消
uint16 ERR_DEPENDENCY_NOT_MET           = 4008   # 前置依赖未满足
uint16 ERR_SCHEDULER_PAUSED             = 4009   # 调度器暂停中
uint16 ERR_SCHEDULER_FAULT              = 4010   # 调度器故障
uint16 ERR_MODULE_NOT_READY             = 4011   # 目标模块未就绪
uint16 ERR_ESTOP_ACTIVE                 = 4012   # 急停激活，拒绝任务
uint16 ERR_FAULT_STATE                  = 4013   # FAULT 状态，拒绝任务
uint16 ERR_TASK_NOT_FOUND               = 4014   # 指定任务不存在
uint16 ERR_TASK_NOT_CANCELLABLE         = 4015   # 任务当前状态不可取消
uint16 ERR_CONCURRENT_TASK_LIMIT        = 4016   # 并发任务数达到上限
uint16 ERR_INTERNAL_ERROR               = 4017   # TE 内部错误
uint16 ERR_TASK_DATA_TAG_FAIL           = 4018   # 任务数据标签生成失败
uint16 ERR_AGENT_CONFIDENCE_MISSING     = 4019   # Agent 决策置信度数据缺失
uint16 ERR_DR_RECORD_FAIL               = 4020   # DR 录制任务数据失败

uint16 error_code
string message
