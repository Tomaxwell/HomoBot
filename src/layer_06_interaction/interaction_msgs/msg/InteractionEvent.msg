# Standardized interaction event (Interaction -> Agent/TE/MP)

builtin_interfaces/Time stamp
string event_id             # Event unique ID
uint8 modality              # Input modality
uint8 MODALITY_VOICE   = 0
uint8 MODALITY_VISION  = 1
uint8 MODALITY_TOUCH   = 2
uint8 MODALITY_MESSAGE = 3
uint8 MODALITY_FUSION  = 4

string intent_category      # "greeting", "command", "query", "confirm", "cancel", "unknown"
string intent_action        # "navigate", "pick", "wave", "dance", "stop"
string text                 # Text content (ASR result or message text)
float32 confidence          # Intent confidence (0.0-1.0)
string target_object        # Target object (e.g., "cup")
string target_location      # Target location (e.g., "kitchen")
string json_payload         # Modality-specific data (JSON)
string source_user_id       # Source user ID
