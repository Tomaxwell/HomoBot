# Recorder state broadcast

uint8 state
uint8 prev_state
builtin_interfaces/Time state_changed_at
string session_id           # Current recording session ID
float32 record_duration_sec # Recorded duration (seconds)
uint64 bytes_recorded       # Recorded bytes
float32 storage_usage_percent  # Storage usage ratio
