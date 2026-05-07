# sm_msgs/msg/TransitionEvent.msg
# 状态转换事件（详细记录）

uint8 from_state
uint8 to_state
builtin_interfaces/Time stamp
string requester_node
string reason
int32 priority
bool accepted
string reject_reason
