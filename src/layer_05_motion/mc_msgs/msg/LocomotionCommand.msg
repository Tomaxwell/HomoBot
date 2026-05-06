# mc_msgs/msg/LocomotionCommand.msg
# 行走/移动控制信号（PnC → MC → LC）

builtin_interfaces/Time stamp
float64 vx                   # 前进速度 [m/s]（前为正）
float64 vy                   # 侧向速度 [m/s]（左为正）
float64 yaw_rate             # 偏航角速度 [rad/s]（逆时针为正）
float64 max_linear_accel     # 最大线加速度 [m/s^2]
float64 max_angular_accel    # 最大角加速度 [rad/s^2]
