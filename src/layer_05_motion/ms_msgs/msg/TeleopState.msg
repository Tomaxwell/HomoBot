# 遥操会话状态广播

uint8 MS_IDLE         = 0
uint8 MS_CONNECTING   = 1
uint8 MS_READY        = 2
uint8 MS_STREAMING    = 3
uint8 MS_PAUSED       = 4
uint8 MS_ERROR        = 5

uint8 state
uint8 prev_state
builtin_interfaces/Time state_changed_at
string device_id
string device_type
bool device_connected
float32 tracking_quality
uint32 frame_count
float32 stream_frequency
