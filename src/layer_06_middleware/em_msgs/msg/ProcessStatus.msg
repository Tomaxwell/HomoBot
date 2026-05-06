uint8 UNKNOWN     = 0
uint8 STARTING    = 1
uint8 RUNNING     = 2
uint8 DEGRADED    = 3
uint8 STOPPED     = 4
uint8 FAILED      = 5
uint8 RESTARTING  = 6

string process_name
string process_group
uint8 state
int32 pid
builtin_interfaces/Time last_heartbeat
float32 cpu_percent
float32 memory_mb
uint32 restart_count
int32 exit_code
