uint8 ANOMALY_URDF_INVALID       = 1
uint8 ANOMALY_FRAME_CONFLICT     = 2
uint8 ANOMALY_INPUT_TIMEOUT      = 3
uint8 ANOMALY_CHAIN_DISCONNECTED = 4
uint8 ANOMALY_TIMESTAMP_JUMP     = 5
uint8 ANOMALY_JOINT_MISSING      = 6
uint8 ANOMALY_RATE_DROP          = 7

uint8 anomaly_type
string frame_id
string child_frame_id
string source
float32 observed_value
float32 threshold_value
string message
builtin_interfaces/Time stamp
