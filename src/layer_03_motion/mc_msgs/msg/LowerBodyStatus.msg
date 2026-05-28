# mc_msgs/msg/LowerBodyStatus.msg
# 下肢状态（LC → MC → 上层）

builtin_interfaces/Time stamp

# 形态无关字段
uint8 robot_type             # 0=bipedal, 1=wheeled
bool is_ground_contact       # 是否有地面接触（足式：任一脚；轮式：底盘着地）
float32 ground_clearance     # 离地间隙 [m]

# 足式专用字段
uint8 gait_phase             # 步态相位（见下方枚举）
float32 gait_progress        # 步态周期进度 0.0~1.0
bool left_foot_contact       # 左足触地
bool right_foot_contact      # 右足触地
float32[] left_contact_force # 左足接触力 [N] (fx, fy, fz)
float32[] right_contact_force# 右足接触力 [N] (fx, fy, fz)
float32 balance_score        # 平衡评分 0.0~1.0（1.0=最稳定）

# 轮式专用字段
float32 chassis_velocity     # 底盘实际线速度 [m/s]
float32 chassis_yaw_rate     # 底盘实际偏航角速度 [rad/s]
float32 lift_column_height   # 升降柱当前高度 [m]
float32 lift_column_target   # 升降柱目标高度 [m]
bool lift_column_in_position # 升降柱是否到位

# 步态相位枚举（足式）
uint8 GAIT_PHASE_DOUBLE_SUPPORT = 0
uint8 GAIT_PHASE_LEFT_SINGLE    = 1
uint8 GAIT_PHASE_RIGHT_SINGLE   = 2
uint8 GAIT_PHASE_LEFT_SWING     = 3
uint8 GAIT_PHASE_RIGHT_SWING    = 4
uint8 GAIT_PHASE_FALLING        = 5
uint8 GAIT_PHASE_FAULT          = 6  # 足式 GROUND_FAULT 或轮式 WHEEL_FAULT
