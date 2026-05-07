# DecisionQuality.msg
# Agent 决策质量报告（供 DataQualityFilter / DR 使用）

builtin_interfaces/Time stamp
string task_id
string skill_name
float32 confidence_score

uint8 quality_tag
uint8 TAG_HIGH    = 0
uint8 TAG_MEDIUM  = 1
uint8 TAG_LOW     = 2

string reason
float32[] token_probabilities
bool consistency_check_passed
