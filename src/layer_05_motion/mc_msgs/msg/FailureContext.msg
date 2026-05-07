# Failure context (frozen N-cycle state snapshot)

string failure_id
builtin_interfaces/Time failure_time
string failure_type
string[] failure_reasons

# Frozen state from FailureCaptureBuffer
float64[] joint_positions
float64[] joint_velocities
float64[] joint_efforts
float64[] control_commands

# Base state
geometry_msgs/Pose base_pose
geometry_msgs/Twist base_twist

# Sensor readings at failure moment
sensor_msgs/Imu imu_reading
float64[] force_sensor_readings

# Diagnosis info
string hds_diagnosis_result
string mc_internal_error_code

# Recovery info
bool can_auto_recover
string recovery_recommendation
builtin_interfaces/Time stamp
