uint8 LIDAR_IDLE       = 0
uint8 LIDAR_INITIALIZING = 1
uint8 LIDAR_READY        = 2
uint8 LIDAR_SCANNING     = 3
uint8 LIDAR_FAULT        = 4

uint8 state
string model_name            # "livox_mid_360s"
string serial_number
float32 temperature_c
uint32 scan_counter          # 累计扫描帧数
builtin_interfaces/Time stamp
