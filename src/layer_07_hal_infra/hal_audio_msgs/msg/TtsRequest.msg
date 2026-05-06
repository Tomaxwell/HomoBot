# TTS synthesis request (internal topic from Interaction)

builtin_interfaces/Time stamp
string text                 # Text to synthesize
string language             # Target language
float32 speed               # Speed (0.5-2.0, 1.0=normal)
float32 pitch               # Pitch (0.5-2.0)
uint8 priority              # Priority (interrupt current playback)
