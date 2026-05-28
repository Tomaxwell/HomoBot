# System event

builtin_interfaces/Time stamp
string event_id               # Event unique ID
string category               # Category ("state_change", "task", "fault", "user_action")
string source_module          # Source module
string description            # Description
uint8 severity                # Severity level
uint8 SEVERITY_INFO     = 0
uint8 SEVERITY_WARNING  = 1
uint8 SEVERITY_ERROR    = 2
uint8 SEVERITY_CRITICAL = 3
string json_payload           # Additional data (JSON)
