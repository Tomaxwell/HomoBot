# mc_msgs/msg/Heartbeat.msg
# MC 心跳（标准模板 + 扩展字段）

builtin_interfaces/Time stamp
string node_name
uint8 state
bool healthy
string status_message

# 扩展字段
uint8 motion_mode
uint8 control_mode
bool uc_healthy                # UC 插件是否正常
bool lc_healthy                # LC 插件是否正常
uint32 control_cycle_count     # 控制周期计数
