uint16 OK                        = 0
uint16 ERR_LLM_UNAVAILABLE       = 5001   # LLM 推理服务不可用
uint16 ERR_LLM_TIMEOUT           = 5002   # LLM 推理超时
uint16 ERR_INVALID_SKILL_CALL    = 5003   # 技能调用参数无效
uint16 ERR_SKILL_EXECUTION_FAIL  = 5004   # 技能执行失败
uint16 ERR_TE_REJECTED           = 5005   # TE 拒绝任务提案
uint16 ERR_PERCEPTION_TIMEOUT    = 5006   # 感知查询超时
uint16 ERR_MAX_ITERATIONS        = 5007   # 达到最大迭代次数
uint16 ERR_CONTEXT_OVERFLOW      = 5008   # 上下文超出 token 限制
uint16 ERR_MEMORY_FAIL           = 5009   # 记忆读写失败

uint16 error_code
string message
