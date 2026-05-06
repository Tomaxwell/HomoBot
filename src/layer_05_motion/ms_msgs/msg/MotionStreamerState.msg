# Motion Streamer state broadcast

uint8 state                 # Current state
uint8 prev_state
builtin_interfaces/Time state_changed_at
float32 output_frequency    # Actual output frequency (Hz)
float32 buffer_usage        # Buffer usage ratio (0.0-1.0)
uint32 active_sources       # Number of active input sources
bool limiting_active        # Whether rate limiting is active
