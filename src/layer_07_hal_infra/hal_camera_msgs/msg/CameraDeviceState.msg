uint8 CAMERA_IDLE       = 0
uint8 CAMERA_INITIALIZING = 1
uint8 CAMERA_READY        = 2
uint8 CAMERA_STREAMING    = 3
uint8 CAMERA_FAULT        = 4

string device_name           # 设备标识（"d435_head", "d405_left", "d405_right"）
uint8 state
string serial_number
float32 temperature_c        # 设备温度
uint32 frame_counter         # 累计帧数
builtin_interfaces/Time stamp
