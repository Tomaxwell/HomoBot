# Shaped motion target (output to MC)

builtin_interfaces/Time stamp
float64[] positions         # Target joint positions (rad)
float64[] velocities        # Target joint velocities (rad/s)
float64[] accelerations     # Target joint accelerations (rad/s^2)
float64[] efforts           # Target joint efforts (Nm)
string[] joint_names
uint8 interpolation_type    # Interpolation type
uint8 INTERP_NONE      = 0
uint8 INTERP_LINEAR    = 1
uint8 INTERP_CUBIC     = 2
uint8 INTERP_MIN_JERK  = 3
