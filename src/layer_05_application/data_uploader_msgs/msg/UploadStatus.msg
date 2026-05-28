# Upload task status

string task_id                    # Upload task unique ID
string data_asset_id              # Corresponding data asset ID
uint8 state                       # Upload state (see state machine enum)
float32 progress_percent          # Upload progress 0.0-100.0
uint64 bytes_uploaded             # Bytes already uploaded
uint64 bytes_total                # Total bytes
float32 upload_speed_bps          # Current upload speed (bytes/s)
uint32 retry_count                # Retry count so far
string error_message              # Error message on failure
builtin_interfaces/Time stamp
