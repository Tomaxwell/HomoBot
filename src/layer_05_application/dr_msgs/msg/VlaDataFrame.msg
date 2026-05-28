# VLA training data frame

builtin_interfaces/Time stamp
sensor_msgs/CompressedImage rgb_image      # RGB image
sensor_msgs/CompressedImage depth_image    # Depth image (optional)
float64[] joint_positions   # Current joint angles (as action label)
float64[] joint_velocities  # Current joint velocities
string language_instruction # Language instruction (e.g., "walk to the table")
string task_phase           # Task phase ("start", "middle", "end")
bool is_keyframe            # Whether this is a keyframe
