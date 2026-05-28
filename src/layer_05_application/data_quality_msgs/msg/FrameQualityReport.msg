# VLA data frame quality report

string frame_id
builtin_interfaces/Time timestamp

bool has_image_left
bool has_image_right
bool has_image_head
bool has_lidar
bool has_joint_state
bool has_language_instruction
bool timestamp_aligned

float32 overall_score
string[] missing_components
string[] quality_issues
