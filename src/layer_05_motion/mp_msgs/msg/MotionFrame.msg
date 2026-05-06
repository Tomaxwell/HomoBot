# Single motion frame data

builtin_interfaces/Time timestamp      # Frame timestamp (relative to motion start)
float64[] joint_positions   # Joint angles (rad)
float64[] joint_velocities  # Joint angular velocities (rad/s), optional
float64[] joint_torques     # Joint torques (Nm), optional
string[] joint_names        # Joint names, corresponding to arrays
bool[] active_joints        # Which joints are active in this frame
