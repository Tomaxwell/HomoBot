# Module health status

string module_name            # Module name
uint8 status                  # Status
uint8 STATUS_ONLINE    = 0
uint8 STATUS_DEGRADED  = 1
uint8 STATUS_OFFLINE   = 2
uint8 STATUS_UNKNOWN   = 3
builtin_interfaces/Time last_heartbeat
float32 heartbeat_freq_hz     # Actual heartbeat frequency
builtin_interfaces/Time uptime_since
