# Motion catalog entry

string motion_id            # Unique motion ID
string name                 # Motion name
string description
float32 duration_sec        # Motion duration (seconds)
uint32 frame_count          # Number of frames
string[] required_states    # Required SM states for execution
float32 complexity_score    # Complexity score
bool is_loopable            # Whether the motion can be looped
