uint8 CMD_POSITION = 0
uint8 CMD_VELOCITY = 1
uint8 CMD_TORQUE   = 2
uint8 CMD_IMPEDANCE = 3

string[] joint_names
float64[] position
float64[] velocity
float64[] effort
float64[] kp
float64[] kd
uint8[] command_modes
builtin_interfaces/Time stamp
