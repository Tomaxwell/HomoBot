# Rule definition (trigger condition + action)

string rule_id                    # Rule unique ID
string name                       # Rule name
string description                # Rule description
uint8 priority                    # Priority (0-255, lower = higher)
string trigger_type               # "topic" / "state" / "time" / "random"
string trigger_condition          # Condition expression string
string action_type                # "dr_record" / "upload" / "both"
uint8 action_duration_before_sec  # Record duration before trigger (seconds)
uint8 action_duration_after_sec   # Record duration after trigger (seconds)
string[] tags                     # Data tags
bool auto_upload                  # Whether to auto-upload
uint8 cooldown_sec                # Cooldown time (seconds)
bool enabled                      # Whether enabled
