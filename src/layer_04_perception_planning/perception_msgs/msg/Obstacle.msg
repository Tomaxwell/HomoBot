uint32 id
uint8 type
uint8 TYPE_UNKNOWN    = 0
uint8 TYPE_PEDESTRIAN = 1
uint8 TYPE_VEHICLE    = 2
uint8 TYPE_CYCLIST    = 3
uint8 TYPE_STATIC     = 4
uint8 TYPE_STAIRS     = 5
uint8 TYPE_RAMP       = 6
string type_confidence

geometry_msgs/Pose pose
geometry_msgs/Vector3 velocity
geometry_msgs/Vector3 size

sensor_msgs/PointCloud2 points
float32 confidence
builtin_interfaces/Time detected_at
