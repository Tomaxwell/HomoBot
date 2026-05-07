# Interaction state broadcast

uint8 state                 # Current state
uint8 prev_state
builtin_interfaces/Time state_changed_at
string active_user_id       # Current interaction user ID (face recognition)
float32 attention_level     # Attention level (0.0-1.0)
uint32 dialogue_turn        # Current dialogue turn
float32 session_duration_sec # Current session duration
