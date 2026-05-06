uint8 IDLE       = 0
uint8 STARTING   = 1
uint8 STOPPING   = 2
uint8 RECOVERING = 3

uint8 phase
string current_action
uint8 progress_percent
string[] running_processes
string[] failed_processes
uint32 restart_count_this_session
