# TaskDataTag.msg
# 任务数据标签（供 DR 录制 VLA 训练数据）

string task_id
string parent_task_id
uint8 task_type

builtin_interfaces/Time start_time
builtin_interfaces/Time end_time

uint8 result_status
uint8 RESULT_UNKNOWN    = 0
uint8 RESULT_SUCCESS    = 1
uint8 RESULT_FAILED     = 2
uint8 RESULT_CANCELLED  = 3

uint16 error_code
string error_message
string requester_node
float32 agent_confidence
string[] skill_calls
string environment_context
