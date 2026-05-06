# mc_msgs/msg/ErrorCode.msg
# MC 错误码（按 interface_standards.md 统一命名，禁止前缀式命名）

uint16 OK                        = 0
uint16 ERR_INVALID_MOTION_MODE   = 3001   # 无效运动模式切换请求
uint16 ERR_INVALID_CTRL_MODE     = 3002   # 无效控制模式切换请求
uint16 ERR_MOTION_NOT_ALLOWED    = 3003   # SM 状态不允许运动
uint16 ERR_BALANCE_LOST          = 3004   # 平衡丢失（倾倒检测）
uint16 ERR_PLUGIN_LOAD_FAILED    = 3005   # 插件加载失败
uint16 ERR_UC_UPDATE_FAILED      = 3006   # UC 插件更新失败
uint16 ERR_LC_UPDATE_FAILED      = 3007   # LC 插件更新失败
uint16 ERR_JOINT_LIMIT_VIOLATION = 3008   # 关节超限
uint16 ERR_TORQUE_SATURATION     = 3009   # 力矩饱和
uint16 ERR_ETHERCAT_TIMEOUT      = 3010   # EtherCAT 通信超时
uint16 ERR_STATE_ESTIMATION_FAIL = 3011   # 状态估计发散
uint16 ERR_GAIT_TRANSITION_FAIL  = 3012   # 步态/移动切换失败
uint16 ERR_ESTOP_ACTIVE          = 3013   # 急停激活中
uint16 ERR_PLUGIN_MISMATCH       = 3014   # 插件与形态不匹配
uint16 ERR_COMMAND_AGGREGATION   = 3015   # 指令聚合冲突（UC/LC 争抢同一关节）

uint16 error_code
string message
