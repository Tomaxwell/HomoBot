# 末端执行器目标（TE/Agent → MC → UC）

string side                    # "left" / "right"
geometry_msgs/Pose target_pose # 目标位姿（基座坐标系）
geometry_msgs/Wrench target_wrench # 目标力/力矩（力控时）
uint8 control_type             # 0=位姿控制, 1=力控制, 2=混合控制
float64 max_velocity           # 最大末端速度 [m/s]
float64 max_acceleration       # 最大末端加速度 [m/s^2]
builtin_interfaces/Duration timeout # 超时时间
builtin_interfaces/Time stamp
