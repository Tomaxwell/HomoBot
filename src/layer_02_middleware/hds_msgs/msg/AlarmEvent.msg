uint8 ALARM_RAISED    = 0
uint8 ALARM_CLEARED   = 1
uint8 ALARM_UPDATED   = 2

uint8 event_type
string alarm_id
uint8 diagnosis_state
string entity_id
string summary
string detail
builtin_interfaces/Time stamp
string[] affected_modules
