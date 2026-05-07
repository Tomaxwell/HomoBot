# sm_msgs/msg/ErrorCode.msg
# SM 模块错误码定义

uint16 OK                               = 0
uint16 ERR_SM_INVALID_TRANSITION        = 1001
uint16 ERR_SM_PRIORITY_TOO_LOW          = 1002
uint16 ERR_SM_ESTOP_ACTIVE              = 1003
uint16 ERR_SM_FAULT_UNACKNOWLEDGED      = 1004
uint16 ERR_SM_OPERATOR_ID_REQUIRED      = 1005
uint16 ERR_SM_PRECONDITION_FAILED       = 1006
uint16 ERR_SM_ALREADY_IN_STATE          = 1007
uint16 ERR_SM_SHUTTING_DOWN             = 1008
uint16 ERR_SM_BOOT_TIMEOUT              = 1009
uint16 ERR_SM_CONCURRENT_TRANSITION     = 1010
uint16 ERR_SM_CHARGING                  = 1011
uint16 ERR_SM_UPDATING                  = 1012
uint16 ERR_SM_DEBUG_MODE                = 1013
uint16 ERR_SM_ZERO_TORQUE_MODE          = 1014
uint16 ERR_SM_DAMPING_MODE              = 1015

uint16 error_code
string message
