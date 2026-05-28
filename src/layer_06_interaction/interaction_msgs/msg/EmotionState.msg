# Robot emotion/expression state (Interaction -> MP/LED)

builtin_interfaces/Time stamp
uint8 emotion
uint8 EMOTION_NEUTRAL   = 0
uint8 EMOTION_HAPPY     = 1
uint8 EMOTION_SAD       = 2
uint8 EMOTION_SURPRISED = 3
uint8 EMOTION_CONFUSED  = 4
uint8 EMOTION_FOCUS     = 5
float32 intensity
string led_pattern
string motion_hint
