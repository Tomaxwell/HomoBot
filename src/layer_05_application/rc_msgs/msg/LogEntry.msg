# Aggregated log entry

builtin_interfaces/Time timestamp
string module_name
string node_name
uint8 level                   # Log level
uint8 LEVEL_DEBUG  = 0
uint8 LEVEL_INFO   = 1
uint8 LEVEL_WARN   = 2
uint8 LEVEL_ERROR  = 3
uint8 LEVEL_FATAL  = 4
string message
string file
uint32 line
