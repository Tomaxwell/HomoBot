# Data asset metadata

string asset_id                   # Asset unique ID
string asset_type                 # "vla_training" / "failure_blackbox" / "operation_log" / "sensor_raw"
string source_module              # Source module (DR / HDS / MC etc.)
string file_path                  # Local file path
uint64 file_size_bytes            # File size
string checksum_sha256            # SHA256 checksum
string[] tags                     # Tags (e.g. ["failure", "auto_capture", "high_quality"])
float32 quality_score             # Data quality score 0.0-1.0
builtin_interfaces/Time created_at
builtin_interfaces/Time stamp
