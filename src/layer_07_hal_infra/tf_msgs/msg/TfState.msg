uint8 TF_INIT          = 0
uint8 TF_LOADING_MODEL = 1
uint8 TF_WAITING_INPUT = 2
uint8 TF_PUBLISHING    = 3
uint8 TF_RELOADING     = 4
uint8 TF_DEGRADED      = 5
uint8 TF_ERROR         = 6

uint8 state
uint8 previous_state
string robot_model_name
string urdf_source
string root_frame
uint32 frame_count
uint32 dynamic_joint_count
uint32 static_transform_count
uint32 published_transform_count
float32 max_transform_age_ms
bool frame_tree_complete
string status_message
builtin_interfaces/Time stamp
