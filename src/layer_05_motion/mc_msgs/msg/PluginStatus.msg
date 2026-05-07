# mc_msgs/msg/PluginStatus.msg
# 插件状态详情

string plugin_name             # 插件名称
string plugin_type             # "upper_body" / "lower_body"
bool is_loaded                 # 是否已加载
bool is_initialized            # 是否已初始化
uint32 update_cycle_count      # 更新周期计数
uint32 error_cycle_count       # 失败周期计数
float32 avg_update_time_ms     # 平均更新时间 [ms]
string last_error              # 最近一次错误信息
builtin_interfaces/Time last_update_time
