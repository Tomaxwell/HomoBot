uint8 BUS_INIT         = 0
uint8 BUS_SCANNING     = 1
uint8 BUS_CONFIGURING  = 2
uint8 BUS_PRE_OP       = 3
uint8 BUS_SAFE_OP      = 4
uint8 BUS_OPERATIONAL  = 5
uint8 BUS_ERROR        = 6
uint8 BUS_RECOVERING   = 7

uint8 bus_state
uint16 slave_count
uint16 online_slave_count
uint32 cycle_count
uint32 error_count
uint32 missed_cycles
float64 cycle_time_ms
float64 max_cycle_jitter_us
builtin_interfaces/Time stamp
