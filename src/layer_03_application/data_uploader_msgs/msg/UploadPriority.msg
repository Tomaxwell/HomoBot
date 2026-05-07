# Upload priority (for queue sorting)

uint8 PRIORITY_CRITICAL = 0       # Failure data, upload immediately
uint8 PRIORITY_HIGH = 1           # Training data, prioritized
uint8 PRIORITY_NORMAL = 2         # Regular logs, upload during idle time
uint8 PRIORITY_LOW = 3            # Diagnostic data, lowest priority

uint8 priority
