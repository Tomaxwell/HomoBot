string proposal_id               # 提案 ID
string description               # 任务自然语言描述
uint8 task_type                  # 任务类型（对应 TE 枚举）
string parameters_json           # 任务参数 JSON
int32 priority                   # 优先级
string reason                    # Agent 生成此任务的原因
builtin_interfaces/Time stamp
