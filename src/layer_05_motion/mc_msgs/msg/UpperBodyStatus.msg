# 上肢状态（UC → MC → 上层）

# 左臂末端
geometry_msgs/Pose left_end_effector_pose
geometry_msgs/Twist left_end_effector_velocity
geometry_msgs/Wrench left_end_effector_wrench
bool left_ik_valid             # 左臂 IK 是否有效

# 右臂末端
geometry_msgs/Pose right_end_effector_pose
geometry_msgs/Twist right_end_effector_velocity
geometry_msgs/Wrench right_end_effector_wrench
bool right_ik_valid            # 右臂 IK 是否有效

# 夹爪状态
bool left_gripper_closed       # 左夹爪是否闭合
float64 left_gripper_position  # 左夹爪开口宽度 [m]
bool right_gripper_closed      # 右夹爪是否闭合
float64 right_gripper_position # 右夹爪开口宽度 [m]

# 自碰检测
bool self_collision_warning    # 接近碰撞阈值
bool self_collision_critical   # 碰撞临界
string collision_pair          # 碰撞关节对名称

# UC 内部状态
uint8 uc_state                 # 当前 UC 状态
uint16 error_code              # 错误码

builtin_interfaces/Time stamp
