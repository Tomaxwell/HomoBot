uint8 BOOTING            = 0
uint8 STANDBY            = 1
uint8 CHARGING           = 2
uint8 UPDATING           = 3
uint8 DEBUG              = 4
uint8 ACTIVE_STAND       = 5
uint8 ACTIVE_READY       = 6
uint8 ACTIVE_SQUAT       = 7
uint8 ACTIVE_SIT         = 8
uint8 ACTIVE_MOTION      = 9
uint8 ACTIVE_WALKING     = 10
uint8 ACTIVE_ZERO_TORQUE = 11
uint8 ACTIVE_DAMPING     = 12
uint8 ACTIVE_E_STOP      = 13
uint8 DEGRADED           = 14
uint8 FAULT              = 15
uint8 SHUTTING_DOWN      = 16

bool is_active
bool is_motion_allowed

uint8 state
uint8 previous_state
builtin_interfaces/Time entered_at
uint32 transition_count
string active_task_id
string last_transition_requester
string last_transition_reason
