uint8 PNC_IDLE       = 0
uint8 PNC_PLANNING   = 1
uint8 PNC_NAVIGATING = 2
uint8 PNC_AVOIDING   = 3
uint8 PNC_REACHED    = 4
uint8 PNC_FAILED     = 5
uint8 PNC_PAUSED     = 6

uint8 state
uint8 previous_state

geometry_msgs/PoseStamped goal_pose
float32 goal_distance_m
float32 path_progress_percent

geometry_msgs/Twist current_cmd

float32 planning_time_ms
uint32 replan_count

builtin_interfaces/Time stamp
