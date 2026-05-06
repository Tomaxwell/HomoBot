uint8 MC_MODE_IDLE          = 0
uint8 MC_MODE_STAND         = 1
uint8 MC_MODE_READY         = 2
uint8 MC_MODE_SQUAT         = 3
uint8 MC_MODE_SIT           = 4
uint8 MC_MODE_MOTION        = 5
uint8 MC_MODE_WALKING       = 6
uint8 MC_MODE_ZERO_TORQUE   = 7
uint8 MC_MODE_DAMPING       = 8

uint8 CTRL_POSITION   = 0
uint8 CTRL_VELOCITY   = 1
uint8 CTRL_TORQUE     = 2
uint8 CTRL_IMPEDANCE  = 3
uint8 CTRL_HYBRID     = 4

uint8 motion_mode
uint8 control_mode
uint8 previous_motion_mode

bool is_walking
bool is_active_control
bool is_balanced

uint8 gait_phase
float32 gait_progress
string stance_foot

builtin_interfaces/Time stamp
