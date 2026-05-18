# 直接关节目标（MP/MS → MC，绕过插件控制算法）

builtin_interfaces/Time stamp
string source_id             # 来源标识: "mp" / "ms"
string[] joint_names         # 目标关节名称列表
float64[] positions          # 关节目标位置 [rad]
float64[] velocities         # 关节目标速度 [rad/s]
float64[] accelerations      # 关节目标加速度 [rad/s^2]
float64[] efforts            # 关节目标力矩 [Nm]
uint8 control_type           # 0=位置优先, 1=速度优先, 2=力矩优先
