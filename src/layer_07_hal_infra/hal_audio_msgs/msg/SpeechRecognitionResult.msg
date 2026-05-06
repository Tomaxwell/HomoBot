# ASR recognition result

builtin_interfaces/Time stamp
string text                 # Recognized text
string language             # Language code ("zh", "en")
float32 confidence          # Confidence (0.0-1.0)
bool is_final               # Is final result
bool is_wake_word           # Is wake word
string wake_word            # Wake word content
float32 audio_duration_sec  # Audio duration
