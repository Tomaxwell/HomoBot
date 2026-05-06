# Audio device state broadcast

uint8 state                 # Current state
uint8 prev_state
builtin_interfaces/Time state_changed_at
bool mic_available          # Microphone available
bool speaker_available      # Speaker available
float32 input_gain_db       # Input gain (dB)
float32 output_volume       # Output volume (0.0-1.0)
uint32 sample_rate_hz       # Current sample rate
uint8 channels              # Number of channels
