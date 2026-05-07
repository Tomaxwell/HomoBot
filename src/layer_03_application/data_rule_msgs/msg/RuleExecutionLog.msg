# Rule execution log

string log_id                     # Log ID
string rule_id                    # Rule ID
builtin_interfaces/Time triggered_at
builtin_interfaces/Time completed_at
string action_result              # "success" / "failed" / "cancelled"
string dr_session_id              # Associated DR recording session ID
string error_message              # Error message
