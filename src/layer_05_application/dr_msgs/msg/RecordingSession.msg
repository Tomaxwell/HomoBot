# Recording session information

string session_id
string name
string description
builtin_interfaces/Time started_at
builtin_interfaces/Time stopped_at
float32 duration_sec
uint64 size_bytes
string[] topics_recorded    # Recorded topic list
string trigger_type         # Trigger type ("manual", "auto_fault", "auto_task", "scheduled")
string task_id              # Associated task ID
string[] tags               # Tags for retrieval
bool is_vla_data            # Whether this is VLA training data
