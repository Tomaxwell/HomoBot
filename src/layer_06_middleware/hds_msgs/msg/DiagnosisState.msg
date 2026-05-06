uint8 HEALTHY    = 0
uint8 WARNING    = 1
uint8 DEGRADED   = 2
uint8 FAULT      = 3
uint8 UNKNOWN    = 4

uint8 state
string entity_id
string entity_type
string entity_name
builtin_interfaces/Time stamp
string reason
uint32 rule_id
float32 confidence
