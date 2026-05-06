# Upstream motion command (input)

string source_id            # Command source identifier ("te", "agent", "pnc")
uint8 priority              # Priority (0-255, higher is better)
float64[] target_positions  # Target joint positions (rad), optional
float64[] target_velocities # Target joint velocities (rad/s), optional
float64[] target_torques    # Target joint torques (Nm), optional
geometry_msgs/Pose base_target_pose   # Base target pose, optional
geometry_msgs/Twist base_target_twist # Base target velocity, optional
builtin_interfaces/Time command_time  # Command effective time
float32 transition_time     # Transition time to this command (s)
string[] active_joints      # Joints controlled by this command
