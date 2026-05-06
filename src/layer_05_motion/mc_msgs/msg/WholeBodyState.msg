geometry_msgs/Point com_position
geometry_msgs/Vector3 com_velocity
geometry_msgs/Quaternion base_orientation
geometry_msgs/Vector3 base_angular_vel

geometry_msgs/Point left_foot_position
geometry_msgs/Point right_foot_position
bool left_foot_contact
bool right_foot_contact
float32[] contact_forces

string[] joint_names
float64[] joint_positions
float64[] joint_velocities
float64[] joint_efforts

builtin_interfaces/Time stamp
