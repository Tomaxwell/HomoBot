# Data Quality Filter error codes
# Range: 9501-9520 (avoid conflict with Perception 9001-9019)

uint16 OK                              = 0
uint16 DQF_ERR_IMAGE_EVAL_FAIL         = 9501   # Image quality evaluation failed
uint16 DQF_ERR_LIDAR_EVAL_FAIL         = 9502   # LiDAR quality evaluation failed
uint16 DQF_ERR_IMU_EVAL_FAIL           = 9503   # IMU quality evaluation failed
uint16 DQF_ERR_VLA_CHECK_FAIL          = 9504   # VLA frame completeness check failed
uint16 DQF_ERR_REDUNDANCY_DETECT_FAIL  = 9505   # Redundancy detection failed
uint16 DQF_ERR_BUFFER_OVERFLOW         = 9506   # Frame buffer overflow
uint16 DQF_ERR_EVAL_TIMEOUT            = 9507   # Single frame evaluation timeout
uint16 DQF_ERR_SENSOR_DATA_MISSING     = 9508   # Sensor data missing
uint16 DQF_ERR_TIMESTAMP_MISALIGN      = 9509   # Timestamp misalignment
uint16 DQF_ERR_THRESHOLD_INVALID       = 9510   # Invalid threshold configuration

uint16 error_code
string message
