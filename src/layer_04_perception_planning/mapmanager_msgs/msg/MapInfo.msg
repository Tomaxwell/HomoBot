string map_id
string name
string description
builtin_interfaces/Time created_at
builtin_interfaces/Time modified_at
string created_by
uint8 map_type
uint8 MAP_TYPE_VISUAL    = 0
uint8 MAP_TYPE_POINTCLOUD = 1
uint8 MAP_TYPE_OCCUPANCY  = 2
uint8 MAP_TYPE_ELEVATION  = 3
uint8 MAP_TYPE_FUSED     = 4
uint64 file_size_bytes
string file_path
geometry_msgs/Point origin
geometry_msgs/Vector3 extent
float32 resolution
bool is_active
uint8 quality_score
string version
