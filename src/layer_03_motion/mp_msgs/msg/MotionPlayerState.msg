# Motion Player State - Playback status broadcast

uint8 state                 # Current state
uint8 prev_state
builtin_interfaces/Time state_changed_at
string current_motion_id    # Currently playing motion ID
string current_motion_name  # Currently playing motion name
float32 progress_percent    # Playback progress (0.0-100.0)
float32 playback_speed      # Current playback speed multiplier
uint32 current_frame        # Current frame number
uint32 total_frames         # Total frame count
