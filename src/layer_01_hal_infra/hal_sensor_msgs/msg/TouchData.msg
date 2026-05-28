string sensor_id             # 传感器标识（"gripper_left", "gripper_right"）
float32[] pressures          # 各触觉点压力值（N）
float32[] temperatures       # 各点温度（C）
geometry_msgs/Point[] contact_positions  # 接触点位置（本地坐标系）
bool[] contact_detected      # 是否检测到接触
uint32 contact_count         # 当前接触点数
builtin_interfaces/Time stamp
