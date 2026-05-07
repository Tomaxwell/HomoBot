# MultimodalSyncFrame.msg
# 六模态硬同步数据帧（供 DR 录制 VLA 训练数据）

builtin_interfaces/Time stamp
string frame_id

# 视觉模态
sensor_msgs/CompressedImage rgb_image_left
sensor_msgs/CompressedImage rgb_image_right
sensor_msgs/CompressedImage rgb_image_head
sensor_msgs/CompressedImage depth_image

# LiDAR模态
sensor_msgs/PointCloud2 point_cloud

# 力觉/触觉模态
float64[] joint_torques          # 关节力矩
float64[] contact_forces         # 足底/末端接触力
float64[] tactile_pressures      # 触觉阵列压力值

# 关节状态（与 MC 对齐）
sensor_msgs/JointState joint_state

# 语言指令（透传，由 Agent 填充）
string language_instruction
string task_phase

# 同步质量元数据
float32 sync_accuracy_ms         # 实际同步精度（ms）
bool is_fully_synced             # 所有模态是否对齐
string[] missing_modalities      # 缺失的模态列表
