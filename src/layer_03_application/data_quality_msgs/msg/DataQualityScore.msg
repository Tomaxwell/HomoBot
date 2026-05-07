# Data quality score

uint8 SENSOR_IMAGE = 0
uint8 SENSOR_LIDAR = 1
uint8 SENSOR_IMU = 2
uint8 SENSOR_FORCE = 3
uint8 SENSOR_JOINT = 4
uint8 FRAME_VLA = 10

uint8 data_type                   # Data type
string data_id                    # Data frame unique ID
float32 overall_score             # Overall quality score 0.0-1.0
float32 sharpness_score           # Image sharpness score
float32 density_score             # Point cloud density score
float32 noise_score               # Noise level score
float32 completeness_score        # Completeness score (VLA frame)
float32 timestamp_align_score     # Timestamp alignment score
bool is_redundant                 # Whether redundant (similar frame)
bool is_dirty                     # Whether dirty data
string[] dirty_reasons            # Dirty data reason list
builtin_interfaces/Time stamp
